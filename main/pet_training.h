#pragma once
#include <stdbool.h>
#include <stdint.h>

#define PET_TRAIN_ROUNDS 3U
#define PET_TRAIN_ROUND_MS 5000U
/* Pure, wrap-safe timing model. Only a physical click records an attempt;
 * leaving a game discards it. No heap, device clock or persistence ownership. */
typedef struct {
    uint32_t started_at, hit_at, seed;
    uint8_t scores[PET_TRAIN_ROUNDS], positions[PET_TRAIN_ROUNDS], attempts;
    bool active, finished;
} pet_training_t;
void pet_training_start(pet_training_t *game, uint32_t now);
void pet_training_tick(pet_training_t *game, uint32_t now);
bool pet_training_hit(pet_training_t *game, uint32_t now);
unsigned pet_training_round(const pet_training_t *game, uint32_t now);
unsigned pet_training_position(const pet_training_t *game, uint32_t now);
unsigned pet_training_score(const pet_training_t *game);
unsigned pet_training_attempts(const pet_training_t *game);

