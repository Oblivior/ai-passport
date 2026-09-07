#include "pet_life.h"
#include "pet_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static pet_usage_t usage(unsigned date, unsigned food)
{
    pet_usage_t u = {.date = date, .daily_goal = 1000, .tokens_today = 900};
    u.earned[date % 100 - 1] = food;
    return u;
}

static void test_three_days(void)
{
    pet_life_t life;
    pet_life_init(&life, NULL);
    assert(!pet_life_eat(&life));
    for (unsigned day = 7; day < 10; day++) {
        pet_usage_t u = usage(20260900 + day, 5);
        assert(pet_life_sync(&life, &u));
        assert(pet_life_pending(&life) == 5);
        for (unsigned i = 0; i < 5; i++) assert(pet_life_eat(&life));
        assert(!pet_life_eat(&life));
        assert(life.stage == day - 6);
        assert(pet_life_days(&life) == day - 6);
        assert(pet_life_valid(&life));
        assert(pet_life_sync(&life, &u));
        assert(pet_life_pending(&life) == 0); /* Replay after reboot/ACK loss. */
    }
    assert(life.stage == PET_STAGE_SCOUT);
    pet_usage_t rest = usage(20260914, 0);
    assert(pet_life_sync(&life, &rest));
    assert(life.stage == PET_STAGE_SCOUT);
    assert(pet_life_meals(&life) == 15);
}

static void test_adoption_and_catchup(void)
{
    pet_life_t life;
    pet_life_init(&life, NULL);
    pet_usage_t u = usage(20260907, 2);
    u.earned[0] = 5;
    assert(pet_life_sync(&life, &u));
    assert(pet_life_pending(&life) == 2); /* Old history does not insta-level. */
    assert(pet_life_eat(&life));
    u.earned[6] = 1; /* Downstream correction does not un-feed a pet. */
    assert(pet_life_sync(&life, &u));
    assert(pet_life_pending(&life) == 1);
    u = usage(20260910, 5);
    u.earned[7] = 3;
    u.earned[8] = 4;
    assert(pet_life_sync(&life, &u));
    assert(pet_life_pending(&life) == 13);
    while (pet_life_eat(&life)) {}
    assert(pet_life_days(&life) == 4);
    pet_usage_t rollback = usage(20260909, 5);
    pet_life_t before = life;
    assert(!pet_life_sync(&life, &rollback));
    assert(!memcmp(&before, &life, sizeof(life)));
    u.daily_goal++;
    assert(!pet_life_sync(&life, &u));
    assert(!memcmp(&before, &life, sizeof(life)));
}

static void test_migration_and_month(void)
{
    pet_model_t legacy;
    pet_model_init(&legacy);
    pet_model_begin_month(&legacy, 2026, 9);
    pet_model_apply_feed(&legacy, 1, 5);
    pet_model_t before = legacy;
    pet_life_t life;
    pet_life_init(&life, &legacy);
    assert(!memcmp(&legacy, &before, sizeof(legacy)));
    assert(life.family.archive_count == 1 && life.legacy_mask == 1);
    assert(life.stage == PET_STAGE_EGG);
    pet_usage_t u = usage(20260930, 5);
    assert(pet_life_sync(&life, &u));
    assert(pet_life_eat(&life));
    u = usage(20261003, 2);
    u.earned[0] = 3;
    u.earned[1] = 4;
    assert(pet_life_sync(&life, &u));
    assert(life.family.archive_count == 2 && life.legacy_mask == 1);
    assert(life.family.archive[1].stage == PET_STAGE_SPARK);
    assert(pet_life_pending(&life) == 9);
    assert(pet_life_meals(&life) == 0 && life.stage == PET_STAGE_EGG);
    assert(pet_life_sync(&life, &u));
    assert(life.family.archive_count == 2);
}

static void test_full_lifecycle(void)
{
    pet_life_t life;
    pet_life_init(&life, NULL);
    for (unsigned day = 1; day <= 16; day++) {
        pet_usage_t u = usage(20260900 + day, 5);
        assert(pet_life_sync(&life, &u));
        while (pet_life_eat(&life)) {}
        assert(life.stage <= (day < 2 ? PET_STAGE_SPARK : day < 3 ? PET_STAGE_BYTE :
                             day < 6 ? PET_STAGE_SCOUT : day < 10 ? PET_STAGE_RANGER :
                             day < 16 ? PET_STAGE_TITAN : PET_STAGE_APEX));
    }
    assert(life.stage == PET_STAGE_APEX);
    for (unsigned month = 10; month < 30; month++) {
        unsigned year = 2026 + (month - 1) / 12;
        unsigned m = (month - 1) % 12 + 1;
        pet_usage_t u = usage(year * 10000 + m * 100 + 1, 1);
        assert(pet_life_sync(&life, &u));
    }
    assert(life.family.archive_count == 12);
    assert(pet_life_valid(&life));
}

static void test_protocol(void)
{
    pet_usage_t u;
    assert(pet_protocol_parse("PET2 SYNC 20260907 1 1000 0000001000000000000000000000000", &u));
    assert(u.earned[6] == 1);
    assert(!pet_protocol_parse("PET2 SYNC 20260907 1 0 0000001000000000000000000000000", &u));
    assert(!pet_protocol_parse("PET2 SYNC 20260907 -1 10 0000001000000000000000000000000", &u));
    assert(!pet_protocol_parse("PET2 SYNC 20260230 1 100 0000001000000000000000000000000", &u));
    assert(!pet_protocol_parse("PET2 SYNC 20260907 1 100 0000001000000000000000000000001", &u));
    assert(!pet_protocol_parse("PET2 SYNC 20260907 1 100 0000006000000000000000000000000", &u));
    assert(!pet_protocol_parse("PET2 SYNC 20260907 999999999999999999999999 100 0000001000000000000000000000000", &u));
    assert(!pet_protocol_parse("PET2 SYNC 20260907 1 100 0000001000000000000000000000000 extra", &u));
    assert(pet_life_date_valid(20280229));
    assert(!pet_life_date_valid(20260229));
}

int main(void)
{
    test_three_days(); test_adoption_and_catchup(); test_migration_and_month();
    test_full_lifecycle(); test_protocol();
    puts("pet_life/protocol: PASS (three days, replay, catchup, migration, calendar, 16-day evolution)");
}
