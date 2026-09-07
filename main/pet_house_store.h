#pragma once
#include "pet_house.h"

/* 1 loaded; 0 absent (migration allowed); -1 damaged/incompatible, fail closed. */
int pet_house_store_load(pet_house_t *house, uint32_t *generation);
bool pet_house_store_save(const pet_house_t *house, uint32_t *generation);
/* Boot after NVS initialization. Corrupt saves block all later mutations. */
bool pet_house_store_boot(pet_house_t *house, uint32_t *generation, bool *blocked);
