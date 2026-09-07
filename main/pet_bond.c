#include "pet_bond.h"
#include <string.h>

void pet_bond_init(pet_bond_t *bond) { memset(bond, 0, sizeof(*bond)); bond->version = PET_BOND_VERSION; }
bool pet_bond_valid(const pet_bond_t *bond)
{
    if (!bond || bond->version != PET_BOND_VERSION || bond->rewarded_today > PET_BOND_DAILY ||
        (bond->reward_date && !pet_life_date_valid(bond->reward_date)) ||
        (!bond->reward_date && bond->rewarded_today)) return false;
    for (unsigned i = 0; i < sizeof(bond->reserved); i++) if (bond->reserved[i]) return false;
    for (unsigned i = 0; i < PET_PARTNER_CAPACITY; i++)
        if (bond->points[i] > PET_BOND_MAX || (bond->points[i] && (!bond->reward_date || !pet_catalog_find(i + 1)))) return false;
    return true;
}
unsigned pet_bond_points(const pet_bond_t *bond, unsigned id)
{
    return pet_bond_valid(bond) && pet_catalog_find(id) ? bond->points[id - 1] : 0;
}
unsigned pet_bond_remaining(const pet_bond_t *bond, uint32_t date)
{
    if (!pet_bond_valid(bond) || !pet_life_date_valid(date) || date < bond->reward_date) return 0;
    return date > bond->reward_date ? PET_BOND_DAILY : PET_BOND_DAILY - bond->rewarded_today;
}
int pet_bond_reward(pet_bond_t *bond, unsigned id, uint32_t date, unsigned score, unsigned attempts)
{
    if (!pet_bond_valid(bond) || !pet_catalog_find(id) || !pet_life_date_valid(date) ||
        date < bond->reward_date || attempts > 3 || score > attempts * 2) return -1;
    if (!attempts || !pet_bond_remaining(bond, date)) return 0;
    if (date > bond->reward_date) { bond->reward_date = date; bond->rewarded_today = 0; }
    unsigned reward = score == 6 ? 3 : score >= 3 ? 2 : 1;
    unsigned room = PET_BOND_MAX - bond->points[id - 1];
    if (reward > room) reward = room;
    bond->points[id - 1] += reward;
    bond->rewarded_today++;
    return (int)reward;
}

