#pragma once
#include "pet_catalog.h"
#include "pet_life.h"

#define PET_BOND_VERSION 1U
#define PET_BOND_MAX 100U
#define PET_BOND_DAILY 3U
/* Independent, lifetime bond per stable species ID; never changes v3 growth.
 * All companions share a single reward budget per trusted calendar date. */
typedef struct {
    uint32_t version, reward_date;
    uint16_t points[PET_PARTNER_CAPACITY];
    uint8_t rewarded_today, reserved[3];
} pet_bond_t;
void pet_bond_init(pet_bond_t *bond);
bool pet_bond_valid(const pet_bond_t *bond);
unsigned pet_bond_points(const pet_bond_t *bond, unsigned id);
unsigned pet_bond_remaining(const pet_bond_t *bond, uint32_t date);
/* Returns points gained, zero for practice/cap, -1 for invalid input. A valid
 * completed game with attempts consumes one daily slot even at maximum bond. */
int pet_bond_reward(pet_bond_t *bond, unsigned id, uint32_t date, unsigned score, unsigned attempts);

