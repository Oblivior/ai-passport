/* Exercise the real bond storage code with NVS transaction fault injection. */
#include "../main/pet_bond_store.c"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bond_record_t slots[2], staged;
static bool present[2], exists, fail_write, fail_commit;
static unsigned staged_slot, writes;
esp_err_t nvs_open(const char *name, int mode, nvs_handle_t *handle)
{
    assert(!strcmp(name, "ai_pet_bond")); /* Never opens the pet or pairing stores. */
    if (!exists && mode == NVS_READONLY) return ESP_ERR_NVS_NOT_FOUND;
    exists = true; *handle = 1; return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *data, size_t *size)
{
    assert(handle == 1); unsigned at = !strcmp(key, "slot1");
    if (!present[at]) return ESP_ERR_NVS_NOT_FOUND;
    assert(*size == sizeof(bond_record_t)); memcpy(data, &slots[at], *size); return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *data, size_t size)
{
    assert(handle == 1 && size == sizeof(staged)); writes++;
    if (fail_write) return 2;
    staged_slot = !strcmp(key, "slot1"); memcpy(&staged, data, size); return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t handle)
{
    assert(handle == 1); if (fail_commit) return 2;
    slots[staged_slot] = staged; present[staged_slot] = true; return ESP_OK;
}
void nvs_close(nvs_handle_t handle) { assert(handle == 1); }
int main(void)
{
    pet_bond_t bond, out; pet_bond_init(&bond);
    uint32_t generation = 0, loaded = 0;
    assert(pet_bond_store_load(&out, &loaded) == 0 && !writes);
    assert(pet_bond_reward(&bond, 1, 20260907, 6, 3) == 3);
    fail_write = true;
    assert(!pet_bond_store_save(&bond, &generation) && !generation);
    fail_write = false; fail_commit = true;
    assert(!pet_bond_store_save(&bond, &generation) && !generation);
    assert(pet_bond_store_load(&out, &loaded) == 0);
    fail_commit = false;
    assert(pet_bond_store_save(&bond, &generation) && generation == 1);
    assert(pet_bond_store_load(&out, &loaded) == 1 && !memcmp(&out, &bond, sizeof(out)));
    assert(pet_bond_reward(&bond, 2, 20260907, 3, 3) == 2);
    fail_commit = true; assert(!pet_bond_store_save(&bond, &generation));
    assert(pet_bond_store_load(&out, &loaded) == 1 && !out.points[1]);
    fail_commit = false; assert(pet_bond_store_save(&bond, &generation));
    slots[0].crc ^= 1;
    assert(pet_bond_store_load(&out, &loaded) == 1 && loaded == 1 && out.points[0] == 3);
    slots[1].crc ^= 1;
    assert(pet_bond_store_load(&out, &loaded) == -1);
    present[0] = present[1] = false; generation = UINT32_MAX - 1;
    assert(pet_bond_store_save(&bond, &generation) && generation == UINT32_MAX);
    assert(pet_bond_reward(&bond, 3, 20260907, 6, 3) == 3);
    assert(pet_bond_store_save(&bond, &generation) && generation == 0);
    assert(pet_bond_store_load(&out, &loaded) == 1 && loaded == 0 && out.points[2] == 3);
    slots[1].bond.version = 2; slots[1].crc = checksum(&slots[1]);
    assert(pet_bond_store_load(&out, &loaded) == -1); /* Future schema blocks fallback. */
    puts("pet_bond_store: PASS (isolated namespace, CRC slots, commit failure, reboot, corruption, generation wrap, future schema)");
}
