#include "pet_service.h"
#include "pet_protocol.h"
#include "pet_store.h"
#include "pet_ble.h"
#include "demo_radio.h"

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "nvs.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

typedef struct {
    uint32_t generation;
    pet_life_t life;
    uint32_t crc;
} record_t;

static SemaphoreHandle_t s_lock;
static QueueHandle_t s_actions;
static pet_snapshot_t s_snapshot;
static uint32_t s_generation;
static bool s_storage_blocked;
static bool s_wireless_request;
static const char *const KEYS[] = {"slot0", "slot1"};

static uint32_t checksum(const record_t *record)
{
    const uint8_t *bytes = (const uint8_t *)record;
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < offsetof(record_t, crc); i++) {
        crc ^= bytes[i];
        for (unsigned b = 0; b < 8; b++) crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}

/* 1: loaded, 0: new install, -1: unreadable; never replace damaged saves. */
static int load(pet_life_t *life)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("ai_pet_v2", NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) return 0;
    if (err != ESP_OK) return -1;
    record_t records[2];
    bool valid[2];
    for (unsigned i = 0; i < 2; i++) {
        memset(&records[i], 0, sizeof(record_t));
        size_t size = sizeof(record_t);
        valid[i] = nvs_get_blob(h, KEYS[i], &records[i], &size) == ESP_OK &&
            size == sizeof(record_t) && records[i].crc == checksum(&records[i]) &&
            pet_life_valid(&records[i].life);
    }
    nvs_close(h);
    if (!valid[0] && !valid[1]) return -1;
    unsigned at = valid[1] && (!valid[0] || records[1].generation > records[0].generation);
    s_generation = records[at].generation;
    *life = records[at].life;
    return 1;
}

static bool save(const pet_life_t *life)
{
    nvs_handle_t h;
    if (nvs_open("ai_pet_v2", NVS_READWRITE, &h) != ESP_OK) return false;
    record_t record;
    memset(&record, 0, sizeof(record));
    record.generation = s_generation + 1;
    record.life = *life;
    record.crc = checksum(&record);
    esp_err_t err = nvs_set_blob(h, KEYS[record.generation % 2], &record, sizeof(record));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) return false;
    s_generation = record.generation;
    return true;
}

static void publish(const pet_snapshot_t *state)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_snapshot = *state;
    xSemaphoreGive(s_lock);
}

static bool commit(pet_snapshot_t *state, const pet_life_t *next)
{
    if (s_storage_blocked || ((!state->storage_ok || memcmp(&state->life, next, sizeof(*next))) && !save(next))) {
        state->storage_ok = false;
        publish(state);
        return false;
    }
    state->life = *next;
    state->storage_ok = true;
    state->revision++;
    publish(state);
    return true;
}

static void reply(const pet_snapshot_t *state, const char *status)
{
    char line[PET_LINE_MAX];
    snprintf(line, sizeof(line), "PET2 %s date=%lu pending=%u meals=%u days=%u stage=%u goal=%llu archives=%u",
        status, (unsigned long)state->life.date, pet_life_pending(&state->life),
        pet_life_meals(&state->life), pet_life_days(&state->life), state->life.stage,
        (unsigned long long)state->life.daily_goal, state->life.family.archive_count);
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
    pet_usage_t usage;
    pet_life_t next = state->life;
    if (!pet_protocol_parse(line, &usage) || !pet_life_sync(&next, &usage)) {
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
    int loaded = nvs_ok ? load(&state.life) : -1;
    if (loaded <= 0) {
        s_storage_blocked = loaded < 0;
        pet_model_t old;
        bool migrated = loaded == 0 && pet_store_load(&old);
        pet_life_init(&state.life, migrated ? &old : NULL);
        state.storage_ok = !s_storage_blocked && save(&state.life);
    } else state.storage_ok = true;
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
        uint8_t action;
        if (xQueueReceive(s_actions, &action, 0) == pdTRUE) {
            pet_life_t next = state.life;
            if (pet_life_eat(&next)) commit(&state, &next);
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
    s_actions = xQueueCreate(1, sizeof(uint8_t));
    if (!s_lock || !s_actions || xTaskCreate(worker, "pet_life", 6144, NULL, 3, NULL) != pdPASS) {
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

bool pet_service_eat(void)
{
    uint8_t action = 1;
    return s_actions && xQueueSend(s_actions, &action, 0) == pdTRUE;
}
