#include "pet_house_store.h"
#include "nvs.h"
#include "pet_store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned char slots[2][2048], staged[2048];
static size_t sizes[2], staged_size;
static unsigned staged_at;
static bool exists, fail_write, fail_commit;
static bool v1_exists, v1_valid, v2_exists;
static unsigned writes;
typedef struct { uint32_t generation; pet_life_t life; uint32_t crc; } legacy_record_t;
static legacy_record_t legacy;
static uint32_t checksum(const void *value, size_t size)
{
    uint32_t crc = UINT32_MAX;
    const uint8_t *data = value;
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (unsigned b = 0; b < 8; b++) crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}
bool pet_store_load(pet_model_t *model)
{
    assert(v1_exists);
    pet_model_init(model);
    pet_model_begin_month(model, 2026, 8);
    pet_model_apply_feed(model, 1, 5);
    return v1_valid;
}
esp_err_t nvs_open(const char *name, int mode, nvs_handle_t *handle)
{
    if (!strcmp(name, "ai_pet_v2") || !strcmp(name, "ai_pet")) {
        assert(mode == NVS_READONLY);
        *handle = !strcmp(name, "ai_pet_v2") ? 2 : 3;
        return (*handle == 2 ? v2_exists : v1_exists) ? ESP_OK : ESP_ERR_NVS_NOT_FOUND;
    }
    assert(!strcmp(name, "ai_pet_v3")); /* No writes to v1/v2/pairing namespaces. */
    if (!exists && mode == NVS_READONLY) return ESP_ERR_NVS_NOT_FOUND;
    exists = true; *handle = 1; return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *data, size_t *size)
{
    if (handle == 2) {
        if (strcmp(key, "slot1")) return ESP_ERR_NVS_NOT_FOUND;
        assert(*size >= sizeof(legacy));
        memcpy(data, &legacy, sizeof(legacy)); *size = sizeof(legacy); return ESP_OK;
    }
    assert(handle == 1);
    unsigned at = !strcmp(key, "slot1");
    if (!sizes[at]) return ESP_ERR_NVS_NOT_FOUND;
    if (!data) { *size = sizes[at]; return ESP_OK; }
    assert(*size >= sizes[at]);
    memcpy(data, slots[at], sizes[at]); *size = sizes[at]; return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *data, size_t size)
{
    assert(handle == 1 && size <= sizeof(staged));
    writes++;
    if (fail_write) return 2;
    staged_at = !strcmp(key, "slot1");
    staged_size = size; memcpy(staged, data, size); return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t handle)
{
    assert(handle == 1);
    if (fail_commit) return 2;
    memcpy(slots[staged_at], staged, staged_size); sizes[staged_at] = staged_size; return ESP_OK;
}
void nvs_close(nvs_handle_t handle) { assert(handle >= 1 && handle <= 3); staged_size = 0; }
int main(void)
{
    pet_house_t house, out;
    uint32_t generation = 0, loaded = 0;
    pet_house_init(&house, NULL);
    assert(pet_house_store_load(&out, &loaded) == 0);
    fail_commit = true;
    assert(!pet_house_store_save(&house, &generation) && generation == 0);
    assert(pet_house_store_load(&out, &loaded) == 0); /* Interrupted initial migration can retry. */
    fail_commit = false;
    assert(pet_house_store_save(&house, &generation) && generation == 1);
    pet_house_choose(&house, PET_AGUMON);
    assert(pet_house_store_save(&house, &generation) && generation == 2);
    assert(pet_house_store_load(&out, &loaded) == 1 && loaded == 2 && out.active_id == PET_AGUMON);
    pet_house_choose(&house, PET_GABUMON);
    fail_write = true;
    assert(!pet_house_store_save(&house, &generation) && generation == 2);
    fail_write = false; fail_commit = true;
    assert(!pet_house_store_save(&house, &generation) && generation == 2);
    assert(pet_house_store_load(&out, &loaded) == 1 && out.active_id == PET_AGUMON);
    fail_commit = false;
    slots[0][24] ^= 1;
    assert(pet_house_store_load(&out, &loaded) == 1 && loaded == 1 && out.active_id == 0);
    slots[1][24] ^= 1;
    assert(pet_house_store_load(&out, &loaded) == -1); /* Caller must block migration and writes. */
    sizes[0] = sizes[1] = 0; generation = UINT32_MAX - 1;
    assert(pet_house_store_save(&house, &generation) && generation == UINT32_MAX);
    pet_house_choose(&house, PET_PATAMON);
    assert(pet_house_store_save(&house, &generation) && generation == 0);
    assert(pet_house_store_load(&out, &loaded) == 1 && loaded == 0 && out.active_id == PET_PATAMON);
    house.active_id = 8;
    assert(!pet_house_store_save(&house, &generation));
    /* A CRC-valid newer catalog must block rollback to the older valid slot. */
    typedef struct { uint32_t generation; pet_house_t house; uint32_t crc; } record_t;
    record_t future = {.generation = 1, .house = out};
    future.house.partners[7].highest_plus_one = 1;
    future.crc = checksum(&future, offsetof(record_t, crc));
    memcpy(slots[1], &future, sizeof(future)); sizes[1] = sizeof(future);
    bool blocked = false;
    unsigned previous_writes = writes;
    assert(!pet_house_store_boot(&out, &loaded, &blocked) && blocked && writes == previous_writes);
    /* Production boot path migrates v2 exactly once, preserves its source. */
    exists = false; sizes[0] = sizes[1] = 0; v2_exists = true;
    memset(&legacy, 0, sizeof(legacy)); legacy.generation = 9;
    pet_life_init(&legacy.life, NULL);
    pet_usage_t usage = {.date = 20260907, .daily_goal = 1000}; usage.earned[6] = 3;
    assert(pet_life_sync(&legacy.life, &usage));
    assert(pet_life_eat(&legacy.life));
    legacy.life.family.archive_count = 1;
    legacy.life.family.archive[0] = (pet_archive_entry_t){.year = 2026, .month = 8, .stage = PET_STAGE_APEX};
    legacy.crc = checksum(&legacy, offsetof(legacy_record_t, crc));
    legacy_record_t source = legacy;
    assert(pet_house_store_boot(&out, &loaded, &blocked) && !blocked && out.active_id == PET_AGUMON);
    assert(pet_house_meals(&out, PET_AGUMON) == 1 && pet_life_pending(&out.life) == 2);
    assert(out.archive_count == 0); /* v2 robot history is not imported into this product. */
    assert(!memcmp(&source, &legacy, sizeof(legacy)));
    assert(pet_house_choose(&out, PET_GABUMON));
    assert(pet_house_store_save(&out, &loaded));
    previous_writes = writes;
    assert(pet_house_store_boot(&out, &loaded, &blocked) && out.active_id == PET_GABUMON && writes == previous_writes);
    /* A corrupt v3 must not re-import the still-valid v2 source. */
    slots[0][24] ^= 1; slots[1][24] ^= 1;
    assert(!pet_house_store_boot(&out, &loaded, &blocked) && blocked && writes == previous_writes);
    exists = false; sizes[0] = sizes[1] = 0; legacy.crc ^= 1;
    assert(!pet_house_store_boot(&out, &loaded, &blocked) && blocked && writes == previous_writes);
    v2_exists = false; v1_exists = true; v1_valid = false;
    assert(!pet_house_store_boot(&out, &loaded, &blocked) && blocked && writes == previous_writes);
    v1_valid = true;
    assert(pet_house_store_boot(&out, &loaded, &blocked) && !blocked && !out.active_id && out.archive_count == 0);
    /* Upgrade an existing mixed v3 archive via normal CRC transactions. */
    assert(pet_house_choose(&out, PET_AGUMON));
    assert(pet_house_sync(&out, &usage));
    assert(pet_house_eat(&out));
    assert(pet_house_choose(&out, PET_GABUMON));
    out.archive_count = 3;
    for (unsigned i = 0; i < 3; i++) out.archive[i] = (pet_house_archive_t){
        .species_id = i, .result = {.year = 2026, .month = 8, .stage = PET_STAGE_APEX}};
    assert(pet_house_store_save(&out, &loaded));
    pet_house_t original = out, cleaned = out;
    assert(pet_house_clear_legacy_archives(&cleaned) == 1);
    uint32_t original_generation = loaded;
    fail_commit = true;
    assert(!pet_house_store_boot(&out, &loaded, &blocked) && blocked);
    assert(loaded == original_generation && !memcmp(&out, &original, sizeof(out)));
    assert(pet_house_store_load(&house, &generation) == 1 && !memcmp(&house, &original, sizeof(house)));
    fail_commit = false; fail_write = true;
    assert(!pet_house_store_boot(&out, &loaded, &blocked) && blocked);
    assert(loaded == original_generation && !memcmp(&out, &original, sizeof(out)));
    fail_write = false;
    assert(pet_house_store_boot(&out, &loaded, &blocked) && !blocked);
    assert(loaded == original_generation + 1 && !memcmp(&out, &cleaned, sizeof(out)));
    previous_writes = writes;
    assert(pet_house_store_boot(&out, &loaded, &blocked) && writes == previous_writes);
    assert(!memcmp(&out, &cleaned, sizeof(out)));
    /* Falling back to an older CRC slot must not resurrect robot history. */
    slots[loaded % 2][24] ^= 1;
    assert(pet_house_store_boot(&out, &loaded, &blocked) && !blocked);
    assert(!memcmp(&out, &cleaned, sizeof(out)));
    /* Exact legacy 640-byte fixture, not a v5 struct with a changed version. */
    typedef struct { uint32_t generation, padding; uint8_t house[624]; uint32_t crc, tail; } old_record_t;
    _Static_assert(sizeof(old_record_t) == 640, "legacy fixture ABI");
    old_record_t v3 = {.generation = 123};
    pet_house_t source_house = cleaned; source_house.version = 3;
    memcpy(v3.house, &source_house, sizeof(v3.house));
    v3.crc = checksum(&v3, offsetof(old_record_t, crc));
    memset(sizes, 0, sizeof(sizes));
    memcpy(slots[1], &v3, sizeof(v3)); sizes[1] = sizeof(v3);
    assert(pet_house_store_load(&out, &loaded) == 2 && loaded == 123);
    assert(!memcmp(&out, &cleaned, sizeof(out)));
    fail_commit = true;
    assert(!pet_house_store_boot(&out, &loaded, &blocked) && blocked && loaded == 123);
    assert(!memcmp(slots[1], &v3, sizeof(v3)));
    fail_commit = false;
    assert(pet_house_store_boot(&out, &loaded, &blocked) && !blocked && loaded == 124);
    assert(!memcmp(&out, &cleaned, sizeof(out)));
    previous_writes = writes;
    assert(pet_house_store_boot(&out, &loaded, &blocked) && writes == previous_writes);
    /* A future valid schema must block fallback, even next to valid v3. */
    record_t v5 = {.generation = 124, .house = cleaned}; v5.house.version = PET_HOUSE_VERSION + 1;
    v5.crc = checksum(&v5, offsetof(record_t, crc));
    memcpy(slots[0], &v5, sizeof(v5)); sizes[0] = sizeof(v5);
    assert(!pet_house_store_boot(&out, &loaded, &blocked) && blocked && writes == previous_writes);
    /* v4 preserves branch choices and lifetime discoveries; no invented history. */
    source_house = cleaned; source_house.version = 4;
    source_house.branch_mask = 1; source_house.branch_seen_mask = 3;
    v3.generation = 199;
    memcpy(v3.house, &source_house, sizeof(v3.house));
    v3.crc = checksum(&v3, offsetof(old_record_t, crc));
    sizes[0] = 0; memcpy(slots[1], &v3, sizeof(v3)); sizes[1] = sizeof(v3);
    assert(pet_house_store_boot(&out, &loaded, &blocked) && !blocked && loaded == 200);
    source_house.version = PET_HOUSE_VERSION;
    assert(!memcmp(&source_house, &out, sizeof(out)));
    assert(!memcmp(slots[1], &v3, sizeof(v3)));
    previous_writes = writes;
    assert(pet_house_store_boot(&out, &loaded, &blocked) && writes == previous_writes);
    sizes[1] = 1000; /* Unknown future record length must also block fallback. */
    assert(!pet_house_store_boot(&out, &loaded, &blocked) && blocked && writes == previous_writes);
    puts("pet_house_store: PASS (v1/v2/v3 migration, byte preservation, CRC slots, failed commits, future schema)");
}
