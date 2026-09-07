#include "pet_training.h"
#include <string.h>

void pet_training_start(pet_training_t *game, uint32_t now)
{
    memset(game, 0, sizeof(*game));
    game->started_at = now;
    game->seed = now % 997U;
    game->active = true;
}
unsigned pet_training_round(const pet_training_t *game, uint32_t now)
{
    unsigned round = (now - game->started_at) / PET_TRAIN_ROUND_MS;
    return round < PET_TRAIN_ROUNDS ? round : PET_TRAIN_ROUNDS - 1;
}
unsigned pet_training_position(const pet_training_t *game, uint32_t now)
{
    unsigned round = pet_training_round(game, now);
    if (game->attempts & (1U << round)) return game->positions[round];
    unsigned period = 1800U - round * 200U;
    unsigned phase = ((now - game->started_at) % PET_TRAIN_ROUND_MS + game->seed + round * 277U) % period;
    unsigned ramp = phase * 200U / period;
    return ramp > 100U ? 200U - ramp : ramp;
}
void pet_training_tick(pet_training_t *game, uint32_t now)
{
    if (game->active && now - game->started_at >= PET_TRAIN_ROUNDS * PET_TRAIN_ROUND_MS) {
        game->active = false;
        game->finished = true;
    }
}
bool pet_training_hit(pet_training_t *game, uint32_t now)
{
    pet_training_tick(game, now);
    if (!game->active) return false;
    unsigned round = pet_training_round(game, now);
    if (game->attempts & (1U << round)) return false;
    unsigned position = pet_training_position(game, now);
    game->positions[round] = position;
    game->scores[round] = position >= 45 && position <= 55 ? 2 : position >= 35 && position <= 65 ? 1 : 0;
    game->attempts |= 1U << round;
    game->hit_at = now;
    if (round == PET_TRAIN_ROUNDS - 1) { game->active = false; game->finished = true; }
    return true;
}
unsigned pet_training_score(const pet_training_t *game)
{
    return game->scores[0] + game->scores[1] + game->scores[2];
}
unsigned pet_training_attempts(const pet_training_t *game)
{
    unsigned count = 0;
    for (unsigned i = 0; i < PET_TRAIN_ROUNDS; i++) count += !!(game->attempts & (1U << i));
    return count;
}

