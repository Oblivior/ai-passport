#include "pet_house_store.h"
#include "pet_store.h"
#include "nvs.h"
#include <stddef.h>
#include <string.h>

typedef struct {
    uint32_t generation;
    pet_house_t house;
    uint32_t crc;
} house_record_t;
_Static_assert(sizeof(pet_house_t) == 624, "Changing the v3 layout requires an explicit migration");
_Static_assert(sizeof(house_record_t) == 640, "Keep the on-flash record ABI stable");
static const char *const KEYS[] = {"slot0", "slot1"};
static uint32_t checksum_bytes(const void *data, size_t length)
{
    const uint8_t *bytes = data;
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < length; i++) {
        crc ^= bytes[i];
        for (unsigned b = 0; b < 8; b++) crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}
static uint32_t checksum(const house_record_t *record)
{
    return checksum_bytes(record, offsetof(house_record_t, crc));
}
int pet_house_store_load(pet_house_t *house, uint32_t *generation)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("ai_pet_v3", NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) return 0;
    if (err != ESP_OK) return -1;
    bool found = false, absent = true, incompatible = false;
    for (unsigned i = 0; i < 2; i++) {
        house_record_t record = {0};
        size_t size = sizeof(record);
        err = nvs_get_blob(handle, KEYS[i], &record, &size);
        if (err != ESP_ERR_NVS_NOT_FOUND) absent = false;
        if (err != ESP_OK || size != sizeof(record) || record.crc != checksum(&record)) continue;
        if (!pet_house_valid(&record.house)) { incompatible = true; continue; }
        if (!found || (int32_t)(record.generation - *generation) > 0) {
            *house = record.house;
            *generation = record.generation;
        }
        found = true;
    }
    nvs_close(handle);
    /* A CRC-valid future species/schema is not corruption: do not downgrade
     * to an older slot and overwrite it with this older catalog. */
    return incompatible ? -1 : found ? 1 : absent ? 0 : -1;
}
bool pet_house_store_save(const pet_house_t *house, uint32_t *generation)
{
    if (!pet_house_valid(house)) return false;
    nvs_handle_t handle;
    if (nvs_open("ai_pet_v3", NVS_READWRITE, &handle) != ESP_OK) return false;
    house_record_t record = {0};
    record.generation = *generation + 1;
    record.house = *house;
    record.crc = checksum(&record);
    esp_err_t err = nvs_set_blob(handle, KEYS[record.generation % 2], &record, sizeof(record));
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    if (err != ESP_OK) return false;
    *generation = record.generation;
    return true;
}

static int load_legacy(pet_life_t *life)
{
    typedef struct { uint32_t generation; pet_life_t life; uint32_t crc; } legacy_record_t;
    nvs_handle_t handle;
    esp_err_t err = nvs_open("ai_pet_v2", NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) return 0;
    if (err != ESP_OK) return -1;
    bool found = false;
    uint32_t generation = 0;
    for (unsigned i = 0; i < 2; i++) {
        legacy_record_t record = {0};
        size_t size = sizeof(record);
        if (nvs_get_blob(handle, KEYS[i], &record, &size) != ESP_OK || size != sizeof(record) ||
            record.crc != checksum_bytes(&record, offsetof(legacy_record_t, crc)) || !pet_life_valid(&record.life)) continue;
        if (!found || (int32_t)(record.generation - generation) > 0) {
            *life = record.life;
            generation = record.generation;
        }
        found = true;
    }
    nvs_close(handle);
    return found ? 1 : -1;
}

bool pet_house_store_boot(pet_house_t *house, uint32_t *generation, bool *blocked)
{
    *generation = 0;
    int loaded = pet_house_store_load(house, generation);
    *blocked = loaded < 0;
    if (loaded == 1) {
        pet_house_t cleaned = *house;
        if (!pet_house_clear_legacy_archives(&cleaned)) return true;
        /* Persist through the normal CRC slots before publishing the cleanup.
         * On failure keep the original state and block writes until reboot;
         * never let a later sync silently save the uncleaned state as success. */
        if (!pet_house_store_save(&cleaned, generation)) { *blocked = true; return false; }
        *house = cleaned;
        return true;
    }
    pet_house_init(house, NULL);
    if (*blocked) return false;
    pet_life_t legacy;
    pet_life_init(&legacy, NULL);
    int previous = load_legacy(&legacy);
    if (previous == 0) {
        nvs_handle_t handle;
        esp_err_t err = nvs_open("ai_pet", NVS_READONLY, &handle);
        if (err == ESP_OK) {
            nvs_close(handle);
            pet_model_t old;
            if (pet_store_load(&old)) pet_life_init(&legacy, &old);
            else previous = -1;
        } else if (err != ESP_ERR_NVS_NOT_FOUND) previous = -1;
    }
    if (previous < 0) { *blocked = true; return false; }
    pet_house_init(house, &legacy);
    pet_house_clear_legacy_archives(house);
    return pet_house_store_save(house, generation);
}
