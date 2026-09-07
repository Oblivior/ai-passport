#pragma once
#include "pet_bond.h"
/* 1 valid, 0 absent, -1 damaged/future: block bond writes, not pet growth. */
int pet_bond_store_load(pet_bond_t *bond, uint32_t *generation);
bool pet_bond_store_save(const pet_bond_t *bond, uint32_t *generation);

