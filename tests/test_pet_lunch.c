#include "pet_lunch.h"
#include <assert.h>
#include <stdio.h>

static unsigned food(uint64_t tokens, uint64_t goal)
{
    uint64_t result = tokens ? (tokens * 5 + goal - 1) / goal : 0;
    return result > 5 ? 5 : (unsigned)result;
}

int main(void)
{
    pet_life_t life;
    pet_life_init(&life, NULL);
    assert(!pet_lunch_read(NULL).available);
    assert(!pet_lunch_read(&life).available);
    life.date = 20260907;
    assert(!pet_lunch_read(&life).available);
    const uint64_t goals[] = {1, 2, 3, 7, 1000, 1003, 1000000000000ULL};
    for (unsigned g = 0; g < sizeof(goals) / sizeof(goals[0]); g++) {
        life.daily_goal = goals[g];
        for (unsigned e = 0; e < 5; e++) {
            life.earned[6] = e;
            life.tokens_today = 0;
            pet_lunch_t lunch = pet_lunch_read(&life);
            assert(lunch.available && lunch.earned == e && !lunch.progress);
            assert(food(lunch.next_tokens - 1, goals[g]) <= e);
            assert(food(lunch.next_tokens, goals[g]) > e);
            assert(lunch.remaining_tokens == lunch.next_tokens);
            life.tokens_today = lunch.next_tokens;
            assert(pet_lunch_read(&life).progress == 100);
            assert(!pet_lunch_read(&life).remaining_tokens);
        }
    }
    life.daily_goal = 1000;
    life.earned[6] = 2;
    life.tokens_today = 300;
    pet_lunch_t lunch = pet_lunch_read(&life);
    assert(lunch.next_tokens == 401 && lunch.remaining_tokens == 101 && lunch.progress == 49);
    life.tokens_today = 10; /* Downward correction keeps the two earned meals. */
    lunch = pet_lunch_read(&life);
    assert(lunch.earned == 2 && lunch.remaining_tokens == 391 && lunch.progress == 0);
    life.earned[5] = 5;
    life.eaten[5] = 3;
    life.eaten[6] = 1;
    lunch = pet_lunch_read(&life);
    assert(lunch.eaten == 1 && lunch.older_pending == 2);
    assert(pet_life_eat(&life));
    lunch = pet_lunch_read(&life);
    assert(lunch.eaten == 1 && lunch.older_pending == 1);
    life.earned[6] = 5;
    life.tokens_today = 9007199254740991ULL;
    lunch = pet_lunch_read(&life);
    assert(lunch.progress == 100 && !lunch.next_tokens && !lunch.remaining_tokens);
    life.earned[6] = 1;
    lunch = pet_lunch_read(&life);
    assert(lunch.progress == 100 && !lunch.remaining_tokens); /* No overflow. */
    life.date = 20260908;
    life.tokens_today = 0;
    lunch = pet_lunch_read(&life);
    assert(!lunch.earned && !lunch.eaten && lunch.next_tokens == 1 && lunch.older_pending == 1);
    life.date = 20260931;
    assert(!pet_lunch_read(&life).available);
    puts("pet_lunch: PASS (thresholds, caps, corrections, source dates, overflow)");
}
