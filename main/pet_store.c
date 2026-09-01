#include "pet_store.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "demo_radio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"

#define PET_STORE_MAGIC 0x50455431U
#define PET_STORE_NAMESPACE "ai_pet"

typedef struct {
    uint32_t magic;
    uint32_t generation;
    pet_model_t model;
    uint32_t crc;
} pet_store_record_t;

static const char *TAG = "pet_store";
static const char *const SLOT_KEYS[] = {"slot0", "slot1"};

static QueueHandle_t s_save_queue;
static uint32_t s_generation;

static uint32_t crc32_bytes(const void *data, size_t length)
{
    const uint8_t *bytes = data;
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < length; i++) {
        crc ^= bytes[i];
        for (uint8_t bit = 0; bit < 8U; bit++) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

static bool record_valid(pet_store_record_t *record)
{
    if (record->magic != PET_STORE_MAGIC) return false;
    uint32_t expected = crc32_bytes(record, offsetof(pet_store_record_t, crc));
    return expected == record->crc && pet_model_sanitize(&record->model);
}

static bool read_slot(nvs_handle_t handle, const char *key, pet_store_record_t *record)
{
    size_t size = sizeof(*record);
    memset(record, 0, sizeof(*record));
    esp_err_t err = nvs_get_blob(handle, key, record, &size);
    return err == ESP_OK && size == sizeof(*record) && record_valid(record);
}

static bool write_model(const pet_model_t *model)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(PET_STORE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "open failed: %s", esp_err_to_name(err));
        return false;
    }

    pet_store_record_t record;
    memset(&record, 0, sizeof(record));
    record.magic = PET_STORE_MAGIC;
    record.generation = ++s_generation;
    record.model = *model;
    record.crc = crc32_bytes(&record, offsetof(pet_store_record_t, crc));

    const char *key = SLOT_KEYS[record.generation % 2U];
    err = nvs_set_blob(handle, key, &record, sizeof(record));
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "save failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

static void save_worker(void *arg)
{
    (void)arg;
    pet_model_t model;
    for (;;) {
        if (xQueueReceive(s_save_queue, &model, portMAX_DELAY) == pdTRUE) {
            write_model(&model);
        }
    }
}

bool pet_store_init(void)
{
    if (s_save_queue) return true;
    if (demo_radio_nvs_prepare() != ESP_OK) return false;

    s_save_queue = xQueueCreate(1, sizeof(pet_model_t));
    if (!s_save_queue) return false;
    if (xTaskCreate(save_worker, "pet_save", 3072, NULL, 3, NULL) != pdPASS) {
        vQueueDelete(s_save_queue);
        s_save_queue = NULL;
        return false;
    }
    return true;
}

bool pet_store_load(pet_model_t *model)
{
    if (!model || !pet_store_init()) return false;

    nvs_handle_t handle;
    if (nvs_open(PET_STORE_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return false;

    pet_store_record_t slots[2];
    bool valid[2] = {
        read_slot(handle, SLOT_KEYS[0], &slots[0]),
        read_slot(handle, SLOT_KEYS[1], &slots[1]),
    };
    nvs_close(handle);

    if (!valid[0] && !valid[1]) return false;
    size_t latest = valid[1] && (!valid[0] || slots[1].generation > slots[0].generation)
                        ? 1U
                        : 0U;
    *model = slots[latest].model;
    s_generation = slots[latest].generation;
    return true;
}

bool pet_store_request_save(const pet_model_t *model)
{
    if (!model || !s_save_queue) return false;
    return xQueueOverwrite(s_save_queue, model) == pdTRUE;
}
