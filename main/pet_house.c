#include "pet_house.h"
#include <string.h>

const pet_partner_t *pet_house_partner(const pet_house_t *house, unsigned id)
{
    return house && id && id <= PET_PARTNER_CAPACITY ? &house->partners[id - 1] : NULL;
}
unsigned pet_house_meals(const pet_house_t *house, unsigned id)
{
    const pet_partner_t *pet = pet_house_partner(house, id);
    unsigned total = 0;
    if (pet) for (unsigned i = 0; i < PET_LIFE_DAYS; i++) total += pet->eaten[i];
    return total;
}
unsigned pet_house_days(const pet_house_t *house, unsigned id)
{
    const pet_partner_t *pet = pet_house_partner(house, id);
    unsigned days = 0;
    bool adoption_day = false;
    if (pet) for (unsigned i = 0; i < PET_LIFE_DAYS; i++) {
        if (!pet->eaten[i]) continue;
        if (i + 1 <= pet->adopted_day) adoption_day = true;
        else days++;
    }
    return days + adoption_day;
}
unsigned pet_house_stage(const pet_house_t *house, unsigned id)
{
    unsigned stage = 0, meals = pet_house_meals(house, id), days = pet_house_days(house, id);
    for (unsigned i = 1; i < PET_STAGE_COUNT; i++) {
        if (meals < pet_catalog_meals(i) || days < pet_catalog_days(i)) break;
        stage = i;
    }
    return stage;
}
static void archive(pet_house_t *house, pet_house_archive_t entry)
{
    if (house->archive_count == PET_ARCHIVE_MAX) {
        memmove(house->archive, house->archive + 1, (PET_ARCHIVE_MAX - 1) * sizeof(entry));
        house->archive_count--;
    }
    house->archive[house->archive_count++] = entry;
}
static void clear_pantry_archive(pet_house_t *house)
{
    memset(house->life.family.archive, 0, sizeof(house->life.family.archive));
    house->life.family.archive_count = 0;
    house->life.legacy_mask = 0;
}
void pet_house_init(pet_house_t *house, const pet_life_t *legacy)
{
    memset(house, 0, sizeof(*house));
    house->version = PET_HOUSE_VERSION;
    pet_life_init(&house->life, NULL);
    if (!legacy) return;
    house->life = *legacy;
    for (unsigned i = 0; i < legacy->family.archive_count; i++) {
        pet_house_archive_t entry = {.result = legacy->family.archive[i]};
        archive(house, entry);
    }
    clear_pantry_archive(house);
    if (legacy->date) {
        house->active_id = PET_AGUMON;
        pet_partner_t *pet = &house->partners[PET_AGUMON - 1];
        pet->adopted = 1;
        pet->adopted_day = legacy->adopted_day;
        memcpy(pet->eaten, legacy->eaten, sizeof(pet->eaten));
        pet->highest_plus_one = pet_house_stage(house, PET_AGUMON) + 1;
    }
}
bool pet_house_valid(const pet_house_t *house)
{
    if (!house || house->version != PET_HOUSE_VERSION || !pet_life_valid(&house->life) ||
        house->life.family.archive_count || house->life.legacy_mask || house->archive_count > PET_ARCHIVE_MAX ||
        house->reserved[0] || house->reserved[1]) return false;
    if (house->active_id && (!pet_catalog_find(house->active_id) ||
        !pet_house_partner(house, house->active_id)->adopted)) return false;
    for (unsigned id = 1; id <= PET_PARTNER_CAPACITY; id++) {
        const pet_partner_t *pet = pet_house_partner(house, id);
        if (pet->adopted > 1 || pet->highest_plus_one > PET_STAGE_COUNT) return false;
        /* Older firmware must not overwrite saves containing unknown species. */
        if ((pet->adopted || pet->highest_plus_one) && !pet_catalog_find(id)) return false;
        if (!pet->adopted && (pet->adopted_day || pet_house_meals(house, id))) return false;
        if (pet->adopted && (pet->highest_plus_one <= pet_house_stage(house, id) ||
            (house->life.date ? (pet->adopted_day < house->life.adopted_day || pet->adopted_day > house->life.date % 100) : pet->adopted_day))) return false;
    }
    for (unsigned d = 0; d < PET_LIFE_DAYS; d++) {
        unsigned allocated = 0;
        for (unsigned id = 1; id <= PET_PARTNER_CAPACITY; id++) allocated += pet_house_partner(house, id)->eaten[d];
        if (allocated != house->life.eaten[d]) return false;
    }
    for (unsigned i = 0; i < house->archive_count; i++) {
        const pet_house_archive_t *entry = &house->archive[i];
        const pet_archive_entry_t *e = &entry->result;
        if ((entry->species_id && !pet_catalog_find(entry->species_id)) || entry->reserved ||
            !pet_life_date_valid(e->year * 10000U + e->month * 100U + 1) ||
            e->stage >= PET_STAGE_COUNT || e->route >= PET_ROUTE_COUNT) return false;
    }
    return true;
}
bool pet_house_sync(pet_house_t *house, const pet_usage_t *usage)
{
    if (!house) return false;
    pet_life_t next = house->life;
    if (!pet_life_sync(&next, usage)) return false;
    bool rollover = house->life.date && next.date / 100 != house->life.date / 100;
    if (rollover) {
        for (unsigned id = 1; id <= PET_PARTNER_CAPACITY; id++) {
            pet_partner_t *pet = &house->partners[id - 1];
            if (pet->adopted) {
                pet_house_archive_t entry = {.species_id = id, .result = {
                    .year = house->life.date / 10000, .month = house->life.date / 100 % 100,
                    .stage = pet_house_stage(house, id), .food_total = pet_house_meals(house, id),
                }};
                archive(house, entry);
            }
            uint8_t discovered = pet->highest_plus_one;
            memset(pet, 0, sizeof(*pet));
            pet->highest_plus_one = discovered;
        }
        house->active_id = PET_SPECIES_NONE;
    }
    house->life = next;
    clear_pantry_archive(house);
    for (unsigned id = 1; id <= PET_PARTNER_CAPACITY; id++) {
        pet_partner_t *pet = &house->partners[id - 1];
        if (pet->adopted && !pet->adopted_day) pet->adopted_day = next.date % 100;
    }
    return true;
}
bool pet_house_choose(pet_house_t *house, unsigned id)
{
    if (!house || !pet_catalog_find(id)) return false;
    pet_partner_t *pet = &house->partners[id - 1];
    if (!pet->adopted) {
        pet->adopted = 1;
        pet->adopted_day = house->life.date % 100;
        if (!pet->highest_plus_one) pet->highest_plus_one = 1;
    }
    house->active_id = id;
    return true;
}
bool pet_house_eat(pet_house_t *house)
{
    if (!house || !house->active_id || !pet_catalog_find(house->active_id) ||
        !pet_house_partner(house, house->active_id)->adopted) return false;
    for (unsigned i = 0; i < PET_LIFE_DAYS; i++) {
        if (house->life.eaten[i] == house->life.earned[i]) continue;
        if (!pet_life_eat(&house->life)) return false;
        pet_partner_t *pet = &house->partners[house->active_id - 1];
        pet->eaten[i]++;
        unsigned discovered = pet_house_stage(house, house->active_id) + 1;
        if (pet->highest_plus_one < discovered) pet->highest_plus_one = discovered;
        return true;
    }
    return false;
}
bool pet_house_action(pet_house_t *house, unsigned expected_id, uint32_t expected_month, unsigned choose_id)
{
    if (!house || expected_id != house->active_id || expected_month != house->life.date / 100) return false;
    return choose_id ? pet_house_choose(house, choose_id) : pet_house_eat(house);
}
