#include "pet_house.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    unsigned ids = 0, artwork = 0;
    for (unsigned i = 0; i < PET_CATALOG_COUNT; i++) {
        const pet_species_info_t *entry = pet_catalog_at(i);
        assert(entry && entry->id && entry->id <= PET_PARTNER_CAPACITY && entry->artwork < PET_CATALOG_COUNT);
        assert(!(ids & (1U << entry->id)) && !(artwork & (1U << entry->artwork)));
        ids |= 1U << entry->id; artwork |= 1U << entry->artwork;
        assert(pet_catalog_find(entry->id) == entry);
        for (unsigned stage = 0; stage < PET_STAGE_COUNT; stage++) assert(entry->forms[stage] && *entry->forms[stage]);
    }
    assert(!pet_catalog_at(PET_CATALOG_COUNT));
    pet_house_t h;
    pet_house_init(&h, NULL);
    assert(pet_house_valid(&h) && !h.active_id && !pet_house_eat(&h));
    assert(!pet_house_choose(&h, 0) && !pet_house_choose(&h, 8) && !pet_house_choose(&h, 300));
    assert(pet_house_choose(&h, PET_AGUMON));
    assert(pet_house_valid(&h));
    pet_usage_t usage = {.date = 20260901, .daily_goal = 1000};
    for (unsigned day = 1; day <= 16; day++) {
        usage.date = 20260900 + day;
        usage.earned[day - 1] = 5;
        assert(pet_house_sync(&h, &usage));
        if (day <= 3) assert(pet_house_eat(&h));
        assert(pet_house_valid(&h));
    }
    assert(pet_house_meals(&h, PET_AGUMON) == 3 && pet_house_days(&h, PET_AGUMON) == 1);
    /* Food uses source dates, not the wall clock of button presses. */
    unsigned shared = pet_life_pending(&h.life);
    assert(pet_house_choose(&h, PET_GABUMON));
    assert(pet_life_pending(&h.life) == shared);
    for (unsigned i = 0; i < 40; i++) assert(pet_house_eat(&h));
    assert(pet_house_meals(&h, PET_GABUMON) == 40 && pet_house_days(&h, PET_GABUMON) == 1);
    assert(pet_house_stage(&h, PET_GABUMON) == PET_STAGE_SPARK);
    for (unsigned i = 0; i < 50; i++) {
        assert(pet_house_choose(&h, PET_AGUMON));
        assert(pet_house_meals(&h, PET_AGUMON) == 3);
        assert(pet_house_choose(&h, PET_GABUMON));
    }
    assert(pet_life_pending(&h.life) == shared - 40);
    pet_house_t before = h;
    assert(!pet_house_action(&h, PET_AGUMON, 202609, 0));
    assert(!pet_house_action(&h, PET_GABUMON, 202608, PET_PATAMON));
    assert(!memcmp(&h, &before, sizeof(h)));
    assert(pet_house_sync(&h, &usage) && !memcmp(&h, &before, sizeof(h)));
    usage.date--;
    assert(!pet_house_sync(&h, &usage) && !memcmp(&h, &before, sizeof(h)));
    usage.date++;
    usage.daily_goal++;
    assert(!pet_house_sync(&h, &usage) && !memcmp(&h, &before, sizeof(h)));
    usage.daily_goal--;
    assert(pet_house_valid(&h));
    assert(pet_house_choose(&h, PET_PATAMON));
    assert(pet_house_stage(&h, PET_PATAMON) == PET_STAGE_EGG);
    usage = (pet_usage_t){.date = 20261001, .daily_goal = 2000, .earned = {5}};
    assert(pet_house_sync(&h, &usage));
    assert(h.archive_count == 3 && h.active_id == 0 && pet_life_pending(&h.life) == 5);
    assert(!pet_house_eat(&h));
    assert(h.archive[0].species_id == PET_AGUMON && h.archive[1].species_id == PET_GABUMON);
    assert(pet_house_partner(&h, PET_GABUMON)->highest_plus_one == 2);
    assert(pet_house_choose(&h, PET_GABUMON));
    assert(pet_house_stage(&h, PET_GABUMON) == 0 && pet_house_meals(&h, PET_GABUMON) == 0);
    assert(pet_house_valid(&h));
    /* Each complete line reaches its own final form with the same rules. */
    for (unsigned index = 0; index < PET_CATALOG_COUNT; index++) {
        unsigned id = pet_catalog_at(index)->id;
        pet_house_init(&h, NULL);
        assert(pet_house_choose(&h, id));
        usage = (pet_usage_t){.date = 20260901, .daily_goal = 1000};
        for (unsigned day = 1; day <= 16; day++) {
            usage.date = 20260900 + day;
            usage.earned[day - 1] = 5;
            assert(pet_house_sync(&h, &usage));
            while (pet_house_eat(&h)) {}
            assert(pet_house_valid(&h));
        }
        assert(pet_house_stage(&h, id) == PET_STAGE_APEX);
        assert(pet_house_partner(&h, id)->highest_plus_one == 7);
        assert(pet_house_meals(&h, id) == 80 && pet_house_days(&h, id) == 16);
    }
    /* v2 migration preserves the active ledger and original archived robots. */
    pet_life_t old = h.life;
    old.family.archive_count = 1;
    old.family.archive[0] = (pet_archive_entry_t){.year = 2026, .month = 8, .stage = PET_STAGE_TITAN};
    assert(pet_life_valid(&old));
    pet_house_init(&h, &old);
    assert(pet_house_valid(&h) && h.active_id == PET_AGUMON);
    assert(pet_house_stage(&h, PET_AGUMON) == PET_STAGE_APEX);
    assert(pet_house_meals(&h, PET_AGUMON) == pet_life_meals(&old));
    assert(pet_house_days(&h, PET_AGUMON) == pet_life_days(&old));
    assert(h.archive_count == 1 && h.archive[0].species_id == 0);
    before = h;
    h.partners[0].eaten[0]++;
    assert(!pet_house_valid(&h));
    h = before; h.active_id = 8; assert(!pet_house_valid(&h));
    h = before; h.partners[7].highest_plus_one = 1; assert(!pet_house_valid(&h));
    h = before; h.partners[0].adopted_day = 32; assert(!pet_house_valid(&h));
    h = before; h.partners[0].highest_plus_one = 0; assert(!pet_house_valid(&h));
    h = before;
    /* Retire only robot history, preserving real archives and every other byte. */
    h.archive_count = 4;
    h.archive[1] = h.archive[0]; h.archive[1].species_id = PET_AGUMON;
    h.archive[2] = h.archive[0];
    h.archive[3] = h.archive[0]; h.archive[3].species_id = PET_GABUMON;
    pet_house_t expected = h;
    expected.archive[0] = h.archive[1]; expected.archive[1] = h.archive[3];
    memset(expected.archive + 2, 0, sizeof(expected.archive) - 2 * sizeof(expected.archive[0]));
    expected.archive_count = 2;
    assert(pet_house_clear_legacy_archives(&h) == 2 && !memcmp(&h, &expected, sizeof(h)));
    assert(pet_house_valid(&h) && pet_house_clear_legacy_archives(&h) == 0);
    h = before;
    assert(pet_house_clear_legacy_archives(&h) == 1 && h.archive_count == 0);
    assert(!memcmp(&h.life, &before.life, sizeof(h.life)));
    assert(!memcmp(h.partners, before.partners, sizeof(h.partners)));
    h = before; h.archive[0].species_id = 8; expected = h;
    assert(!pet_house_clear_legacy_archives(&h) && !memcmp(&h, &expected, sizeof(h)));
    h = before;
    for (unsigned month = 10; month <= 12; month++) {
        usage = (pet_usage_t){.date = 20260001 + month * 100, .daily_goal = 1000};
        assert(pet_house_sync(&h, &usage));
        for (unsigned id = 1; id <= 3; id++) assert(pet_house_choose(&h, id));
    }
    for (unsigned month = 1; month <= 5; month++) {
        usage = (pet_usage_t){.date = 20270001 + month * 100, .daily_goal = 1000};
        assert(pet_house_sync(&h, &usage));
        for (unsigned id = 1; id <= 3; id++) assert(pet_house_choose(&h, id));
        assert(pet_house_valid(&h));
    }
    assert(h.archive_count == PET_ARCHIVE_MAX && h.archive[0].result.year == 2027);
    printf("pet_house: PASS (three lines, shared meals, independent saves, late adoption, migration, rollover); %zu bytes\n", sizeof(h));
}
