#include "pet_house.h"
#include "pet_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static pet_house_t month_with_partners(void)
{
    pet_house_t h;
    pet_house_init(&h, NULL);
    assert(pet_house_choose(&h, PET_AGUMON));
    assert(pet_house_branch_choose(&h, PET_AGUMON, 0, 1, 30));
    assert(pet_house_choose(&h, PET_GABUMON));
    assert(pet_house_choose(&h, PET_PATAMON)); /* Untouched egg stays an egg. */
    pet_usage_t u = {.date = 20260901, .daily_goal = 1000};
    for (unsigned day = 1; day <= 16; day++) {
        u.date = 20260900 + day; u.earned[day - 1] = 3;
        assert(pet_house_sync(&h, &u));
        assert(pet_house_choose(&h, PET_AGUMON));
        assert(pet_house_eat(&h) && pet_house_eat(&h));
        assert(pet_house_choose(&h, PET_GABUMON));
        assert(pet_house_eat(&h));
    }
    u = (pet_usage_t){.date = 20261007, .daily_goal = 2000};
    u.earned[6] = 3;
    assert(pet_house_sync(&h, &u));
    assert(pet_house_choose(&h, PET_PATAMON));
    assert(pet_house_eat(&h));
    assert(pet_house_valid(&h));
    return h;
}
int main(void)
{
    pet_house_t h = month_with_partners(), before = h;
    assert(h.archive_count == 3 && h.months[0].care_days == 16 && h.months[0].month_meals == 48);
    assert(h.months[0].daily_goal == 1000 && h.months[0].chosen_branch == 1);
    assert(!pet_house_settle(&h, 202609, 30000, 20260929)); /* Incomplete month. */
    assert(!pet_house_settle(&h, 202610, 30000, 20261007)); /* Current month. */
    assert(!pet_house_settle(&h, 202609, 30000, 20261008)); /* Future watermark. */
    assert(!pet_house_settle(&h, 202608, 30000, 20260930)); /* No such archive. */
    assert(!pet_house_settle(&h, 202613, 30000, 20260930));
    assert(!pet_house_settle(&h, 202609, UINT64_MAX, 20260930));
    assert(!memcmp(&h, &before, sizeof(h)));
    assert(pet_house_settle(&h, 202609, 18000, 20260930));
    assert(pet_house_valid(&h));
    assert(h.archive[0].result.stage == 6 && h.archive[0].branch == 1);
    assert(h.archive[1].result.stage == 5 && !h.archive[1].branch); /* 30, not 90 meals. */
    assert(!h.archive[2].result.stage && !h.archive[2].result.food_total);
    assert(h.archive[0].result.food_total == 32 && h.archive[1].result.food_total == 16);
    assert(!memcmp(&h.life, &before.life, sizeof(h.life)));
    assert(h.active_id == before.active_id && h.branch_mask == before.branch_mask);
    for (unsigned i = 0; i < PET_PARTNER_CAPACITY; i++) {
        assert(h.partners[i].adopted == before.partners[i].adopted);
        assert(h.partners[i].adopted_day == before.partners[i].adopted_day);
        assert(!memcmp(h.partners[i].eaten, before.partners[i].eaten, PET_LIFE_DAYS));
    }
    before = h;
    assert(pet_house_settle(&h, 202609, 18000, 20261006));
    assert(!memcmp(&h, &before, sizeof(h)));
    assert(pet_house_settle(&h, 202609, 0, 20261006)); /* Downward correction. */
    assert(!memcmp(&h, &before, sizeof(h)));
    /* Evicted siblings never make the surviving one's share larger. */
    memmove(h.archive, h.archive + 1, 2 * sizeof(h.archive[0]));
    memmove(h.months, h.months + 1, 2 * sizeof(h.months[0]));
    memset(h.archive + 2, 0, sizeof(h.archive[0]));
    memset(h.months + 2, 0, sizeof(h.months[0])); h.archive_count = 2;
    assert(pet_house_valid(&h));
    assert(pet_house_settle(&h, 202609, 18000, 20261007));
    assert(h.archive[0].result.stage == 5 && h.months[0].month_meals == 48);
    assert(pet_house_settle(&h, 202609, 9007199254740991ULL, 20261007));
    assert(h.archive[0].result.stage == 6 && !h.archive[1].result.stage);
    /* No invented care days, including a one-day late adoption. */
    h = month_with_partners(); h.months[0].care_days = 1; h.archive[0].result.stage = 1; h.archive[0].branch = 0;
    assert(pet_house_settle(&h, 202609, 30000, 20260930));
    assert(h.archive[0].result.stage == 1);
    h = month_with_partners();
    h.archive[0].result.stage = 4; h.archive[0].result.food_total = 16;
    h.archive[0].branch = 0; h.branch_seen_mask = 0;
    for (unsigned i = 0; i < 3; i++) h.months[i].month_meals = 32;
    assert(pet_house_settle(&h, 202609, 30000, 20260930));
    assert(h.archive[0].result.stage == 6 && h.branch_seen_mask == 3); /* Both crossed dark forms discovered. */
    h = month_with_partners();
    assert(pet_house_settle(&h, 202609, 0, 20260930));
    assert(h.months[0].status == PET_MONTH_SETTLED && !h.months[0].official_tokens);
    before = h; h.months[1].month_meals++;
    assert(!pet_house_valid(&h)); h = before;
    memset(h.months, 0, sizeof(h.months)); before = h;
    assert(!pet_house_settle(&h, 202609, 30000, 20260930)); /* No guessed legacy denominator. */
    assert(!memcmp(&h, &before, sizeof(h)));
    uint32_t month = 0, covered = 0; uint64_t tokens = 0;
    assert(pet_protocol_settlement("PET2 SETTLE 202609 123 20260930", &month, &tokens, &covered));
    assert(month == 202609 && tokens == 123 && covered == 20260930);
    const char *bad[] = {"PET2 SETTLE 202609 -1 20260930", "PET2 SETTLE 202609 NA 20260930",
        "PET2 SETTLE 202609 1.5 20260930", "PET2 SETTLE 202609 1 20260931", "PET2 SETTLE 202609 1 20260930 extra",
        "PET2 SETTLE 202609 9007199254740992 20260930", "PET2 SETTLE 202609 1 2026093"};
    for (unsigned i = 0; i < sizeof(bad) / sizeof(*bad); i++) {
        assert(!pet_protocol_settlement(bad[i], &month, &tokens, &covered));
        assert(month == 202609 && tokens == 123 && covered == 20260930);
    }
    puts("pet_settlement: PASS (shared cap, no regression, no invented care, retry, eviction, coverage, strict protocol)");
}
