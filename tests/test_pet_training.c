#include "pet_training.h"
#include "pet_bond.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    pet_training_t game;
    pet_training_start(&game, UINT32_MAX - 100);
    assert(pet_training_round(&game, 100) == 0);
    assert(pet_training_hit(&game, 100));
    assert(!pet_training_hit(&game, 101));
    assert(pet_training_attempts(&game) == 1);
    pet_training_tick(&game, (UINT32_MAX - 100) + 15000U);
    assert(game.finished && !game.active && !pet_training_hit(&game, 15000));
    for (unsigned round = 0; round < 3; round++) {
        for (unsigned ms = 0; ms < 5000; ms++) {
            pet_training_start(&game, 10);
            unsigned now = 10 + round * 5000 + ms;
            unsigned position = pet_training_position(&game, now);
            assert(position <= 100 && pet_training_round(&game, now) == round);
            assert(pet_training_hit(&game, now));
            assert(game.scores[round] == (position >= 45 && position <= 55 ? 2 : position >= 35 && position <= 65 ? 1 : 0));
            assert(pet_training_position(&game, now + 1) == position || ms == 4999);
            assert(pet_training_attempts(&game) == 1);
        }
    }
    pet_training_start(&game, 0);
    for (unsigned round = 0; round < 3; round++) {
        unsigned now = round * 5000;
        while (pet_training_position(&game, now) != 50) now++;
        assert(pet_training_hit(&game, now));
    }
    assert(game.finished && pet_training_score(&game) == 6 && pet_training_attempts(&game) == 3);
    pet_training_start(&game, 0); pet_training_tick(&game, 14999);
    assert(game.active); pet_training_tick(&game, 15000);
    assert(game.finished && !pet_training_attempts(&game));

    pet_bond_t bond, before;
    pet_bond_init(&bond);
    assert(pet_bond_valid(&bond));
    assert(pet_bond_remaining(&bond, 20260907) == 3);
    before = bond;
    assert(pet_bond_reward(&bond, PET_AGUMON, 20260907, 0, 0) == 0 && !memcmp(&before, &bond, sizeof(bond)));
    assert(pet_bond_reward(&bond, PET_AGUMON, 20260907, 0, 3) == 1);
    assert(pet_bond_reward(&bond, PET_GABUMON, 20260907, 3, 3) == 2);
    assert(pet_bond_reward(&bond, PET_PATAMON, 20260907, 6, 3) == 3);
    before = bond;
    assert(pet_bond_reward(&bond, PET_AGUMON, 20260907, 6, 3) == 0 && !memcmp(&before, &bond, sizeof(bond)));
    assert(pet_bond_reward(&bond, PET_AGUMON, 20260906, 6, 3) == -1 && !memcmp(&before, &bond, sizeof(bond)));
    assert(pet_bond_reward(&bond, PET_AGUMON, 20261001, 6, 3) == 3);
    assert(bond.points[0] == 4 && bond.points[1] == 2 && bond.points[2] == 3 && bond.rewarded_today == 1);
    bond.points[0] = 99;
    assert(pet_bond_reward(&bond, PET_AGUMON, 20261001, 6, 3) == 1 && bond.points[0] == 100);
    assert(pet_bond_reward(&bond, PET_AGUMON, 20261001, 6, 3) == 0 && bond.rewarded_today == 3);
    before = bond;
    assert(pet_bond_reward(&bond, 8, 20261002, 6, 3) == -1);
    assert(pet_bond_reward(&bond, 1, 20260230, 6, 3) == -1);
    assert(pet_bond_reward(&bond, 1, 20261002, 6, 1) == -1);
    assert(!memcmp(&before, &bond, sizeof(bond)));
    bond.points[7] = 1; assert(!pet_bond_valid(&bond));
    bond = before; bond.reserved[0] = 1; assert(!pet_bond_valid(&bond));
    bond = before; bond.version++; assert(!pet_bond_valid(&bond));
    puts("pet_training/bond: PASS (timing, wrap, accuracy, duplicate clicks, daily shared cap, lifetime, saturation, invalid inputs)");
}
