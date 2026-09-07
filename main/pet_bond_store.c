#include "pet_bond_store.h"
#include "nvs.h"
#include <stddef.h>

typedef struct { uint32_t generation; pet_bond_t bond; uint32_t crc; } bond_record_t;
_Static_assert(sizeof(pet_bond_t) == 28 && sizeof(bond_record_t) == 36, "Bond save ABI requires explicit migration");
static const char *const KEYS[] = {"slot0", "slot1"};
static uint32_t checksum(const bond_record_t *record)
{
    const uint8_t *data = (const uint8_t *)record;
    uint32_t crc = UINT32_MAX;
    for (unsigned i = 0; i < offsetof(bond_record_t, crc); i++) {
        crc ^= data[i];
        for (unsigned b = 0; b < 8; b++) crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}
int pet_bond_store_load(pet_bond_t *bond, uint32_t *generation)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("ai_pet_bond", NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) return 0;
    if (err != ESP_OK) return -1;
    bool found = false, absent = true, incompatible = false;
    for (unsigned i = 0; i < 2; i++) {
        bond_record_t record = {0};
        size_t size = sizeof(record);
        err = nvs_get_blob(handle, KEYS[i], &record, &size);
        if (err != ESP_ERR_NVS_NOT_FOUND) absent = false;
        if (err != ESP_OK || size != sizeof(record) || record.crc != checksum(&record)) continue;
        if (!pet_bond_valid(&record.bond)) { incompatible = true; continue; }
        if (!found || (int32_t)(record.generation - *generation) > 0) {
            *bond = record.bond;
            *generation = record.generation;
        }
        found = true;
    }
    nvs_close(handle);
    return incompatible ? -1 : found ? 1 : absent ? 0 : -1;
}
bool pet_bond_store_save(const pet_bond_t *bond, uint32_t *generation)
{
    if (!pet_bond_valid(bond)) return false;
    nvs_handle_t handle;
    if (nvs_open("ai_pet_bond", NVS_READWRITE, &handle) != ESP_OK) return false;
    bond_record_t record = {.generation = *generation + 1, .bond = *bond};
    record.crc = checksum(&record);
    esp_err_t err = nvs_set_blob(handle, KEYS[record.generation % 2], &record, sizeof(record));
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    if (err != ESP_OK) return false;
    *generation = record.generation;
    return true;
}
