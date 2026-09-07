#include "pet_service.h"
#include "pet_protocol.h"
#include "pet_house_store.h"
#include "pet_ble.h"
#include "demo_radio.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

typedef struct {
    uint32_t month;
    uint8_t expected_id;
    uint8_t choose_id; /* Zero feeds, otherwise selects this species. */
} action_t;

static SemaphoreHandle_t s_lock;
static QueueHandle_t s_actions;
static pet_snapshot_t s_snapshot;
static uint32_t s_generation;
static bool s_storage_blocked;
static bool s_wireless_request;
static void publish(const pet_snapshot_t *state)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_snapshot = *state;
    xSemaphoreGive(s_lock);
}

static bool commit(pet_snapshot_t *state, const pet_house_t *next)
{
    if (s_storage_blocked || ((!state->storage_ok || memcmp(&state->house, next, sizeof(*next))) &&
        !pet_house_store_save(next, &s_generation))) {
        state->storage_ok = false;
        publish(state);
        return false;
    }
    state->house = *next;
    state->storage_ok = true;
    state->revision++;
    publish(state);
    return true;
}

static void reply(const pet_snapshot_t *state, const char *status)
{
    char line[PET_LINE_MAX];
    snprintf(line, sizeof(line), "PET2 %s date=%lu pending=%u meals=%u days=%u stage=%u goal=%llu archives=%u",
        status, (unsigned long)state->house.life.date, pet_life_pending(&state->house.life),
        pet_house_meals(&state->house, state->house.active_id), pet_house_days(&state->house, state->house.active_id),
        pet_house_stage(&state->house, state->house.active_id),
        (unsigned long long)state->house.life.daily_goal, state->house.archive_count);
    if (s_wireless_request) pet_ble_reply(line);
    else { printf("\n%s\n", line); fflush(stdout); }
}

static void handle_line(pet_snapshot_t *state, const char *line)
{
    if (!s_wireless_request && !strcmp(line, "PET2 LINK")) {
        pet_ble_status_t ble;
        pet_ble_status(&ble);
        printf("\nPET2 LINK paired=%u ready=%u connected=%u auth=%u error=%d heap=%u largest=%u\n",
            ble.paired, ble.ready, ble.connected, ble.authenticated, ble.error,
            (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
            (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
        fflush(stdout);
        return;
    }
    if (!s_wireless_request && !strncmp(line, "PET2 PAIR ", 10)) {
        bool ok = pet_ble_pair(line + 10);
        pet_ble_status_t ble;
        pet_ble_status(&ble);
        printf("\nPET2 %s id=%s\n", ok ? "PAIRED" : "PAIR_ERROR", ble.id);
        fflush(stdout);
        return;
    }
    if (!strcmp(line, "PET2 STATUS")) {
        reply(state, state->storage_ok ? "STATUS" : "STORAGE_ERROR");
        return;
    }
    if (!strcmp(line, "PET2 ROUTE")) {
        char response[96];
        snprintf(response, sizeof(response), "PET2 ROUTE route=%u locked=%u stage=%u",
            (unsigned)pet_life_route(&state->house.life), pet_life_route_locked(&state->house.life),
            pet_house_stage(&state->house, state->house.active_id));
        if (s_wireless_request) pet_ble_reply(response);
        else { printf("\n%s\n", response); fflush(stdout); }
        return;
    }
    if (!s_wireless_request && !strcmp(line, "PET2 HOUSE")) {
        char response[96];
        unsigned adopted = 0;
        for (unsigned id = 1; id <= PET_PARTNER_CAPACITY; id++)
            if (pet_house_partner(&state->house, id)->adopted) adopted |= 1U << (id - 1);
        snprintf(response, sizeof(response), "PET2 HOUSE version=3 active=%u adopted=%u storage=%u",
            state->house.active_id, adopted, state->storage_ok);
        if (s_wireless_request) pet_ble_reply(response);
        else { printf("\n%s\n", response); fflush(stdout); }
        return;
    }
    pet_usage_t usage;
    pet_house_t next = state->house;
    if (!pet_protocol_parse(line, &usage) || !pet_house_sync(&next, &usage)) {
        reply(state, "REJECTED");
        return;
    }
    if (!commit(state, &next)) {
        reply(state, "STORAGE_ERROR");
        return;
    }
    state->synced_at = (uint32_t)(esp_timer_get_time() / 1000);
    state->synced_wirelessly = s_wireless_request;
    publish(state);
    reply(state, "ACK"); /* ACK is sent only after persistent commit. */
}

static void worker(void *arg)
{
    (void)arg;
    pet_snapshot_t state = {0};
    bool nvs_ok = demo_radio_nvs_prepare() == ESP_OK;
    pet_house_init(&state.house, NULL);
    s_storage_blocked = !nvs_ok;
    state.storage_ok = nvs_ok && pet_house_store_boot(&state.house, &s_generation, &s_storage_blocked);
    state.ready = true;
    state.revision = 1;
    publish(&state);
    /* IDF 5.5 VFS nonblocking reads use the driver's RX ring buffer. The
     * default polling console is enough for logs, but not O_NONBLOCK RX. */
    usb_serial_jtag_driver_config_t usb = {
        .rx_buffer_size = 512,
        .tx_buffer_size = 512,
    };
    if (!usb_serial_jtag_is_driver_installed() && usb_serial_jtag_driver_install(&usb) != ESP_OK) {
        ESP_LOGE("pet_life", "USB sync unavailable: driver initialization failed");
    } else {
        usb_serial_jtag_vfs_use_driver();
    }
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    pet_ble_init();
    char line[PET_LINE_MAX];
    size_t used = 0;
    bool discard = false;
    uint32_t last_byte = 0;
    for (;;) {
        char wireless_line[PET_LINE_MAX];
        if (pet_ble_receive(wireless_line, sizeof(wireless_line))) {
            s_wireless_request = true;
            handle_line(&state, wireless_line);
            s_wireless_request = false;
        }
        unsigned now = (unsigned)(esp_timer_get_time() / 1000);
        if (used && now - last_byte > 5000) { used = 0; discard = true; }
        action_t action;
        if (xQueueReceive(s_actions, &action, 0) == pdTRUE) {
            pet_house_t next = state.house;
            /* A delayed button must never feed another pet or next month's egg. */
            if (pet_house_action(&next, action.expected_id, action.month, action.choose_id)) commit(&state, &next);
        }
        char c;
        for (unsigned i = 0; i < PET_LINE_MAX && read(STDIN_FILENO, &c, 1) == 1; i++) {
            last_byte = now;
            if (c == '\r') continue;
            if (c == '\n') {
                if (!discard && used) {
                    line[used] = 0;
                    handle_line(&state, line);
                }
                used = 0;
                discard = false;
            } else if (!discard) {
                if (used + 1 >= sizeof(line) || c < 32 || c > 126) discard = true;
                else line[used++] = c;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

bool pet_service_start(void)
{
    if (s_actions) return true;
    s_lock = xSemaphoreCreateMutex();
    s_actions = xQueueCreate(1, sizeof(action_t));
    if (!s_lock || !s_actions || xTaskCreate(worker, "pet_life", 8192, NULL, 3, NULL) != pdPASS) {
        if (s_actions) vQueueDelete(s_actions);
        if (s_lock) vSemaphoreDelete(s_lock);
        s_actions = NULL;
        s_lock = NULL;
        return false;
    }
    return true;
}

bool pet_service_snapshot(pet_snapshot_t *snapshot)
{
    if (!s_lock || !snapshot || xSemaphoreTake(s_lock, 0) != pdTRUE) return false;
    *snapshot = s_snapshot;
    xSemaphoreGive(s_lock);
    return snapshot->ready;
}

static bool queue_action(const pet_snapshot_t *shown, unsigned choose_id)
{
    if (!s_actions || !shown || !shown->ready) return false;
    action_t action = {.month = shown->house.life.date / 100,
        .expected_id = shown->house.active_id, .choose_id = choose_id};
    return xQueueSend(s_actions, &action, 0) == pdTRUE;
}
bool pet_service_eat(const pet_snapshot_t *shown) { return queue_action(shown, 0); }
bool pet_service_choose(const pet_snapshot_t *shown, unsigned id) { return pet_catalog_find(id) && queue_action(shown, id); }
