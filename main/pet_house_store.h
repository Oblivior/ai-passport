#pragma once
#include "pet_house.h"

/* 1 loaded; 2 validated v3 upgrade (must commit); 0 absent; -1 fail closed. */
int pet_house_store_load(pet_house_t *house, uint32_t *generation);
bool pet_house_store_save(const pet_house_t *house, uint32_t *generation);
/* Boot after NVS initialization. Remove obsolete robot archives transactionally.
 * Corrupt saves or failed cleanup block mutations until a successful reboot. */
bool pet_house_store_boot(pet_house_t *house, uint32_t *generation, bool *blocked);
