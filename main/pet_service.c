#include "pet_service.h"
#include "pet_protocol.h"
#include "pet_store.h"
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
    printf("\nPET2 %s date=%lu pending=%u meals=%u days=%u stage=%u goal=%llu archives=%u\n",
        status, (unsigned long)state->life.date, pet_life_pending(&state->life),
        pet_life_meals(&state->life), pet_life_days(&state->life), state->life.stage,
        (unsigned long long)state->life.daily_goal, state->life.family.archive_count);
    fflush(stdout);
}

static void handle_line(pet_snapshot_t *state, const char *line)
{
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
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    char line[PET_LINE_MAX];
    size_t used = 0;
    bool discard = false;
    uint32_t last_byte = 0;
    for (;;) {
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
