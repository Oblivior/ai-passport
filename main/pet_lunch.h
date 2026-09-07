#pragma once

#include "pet_life.h"

/* Read-only projection of the saved source date, never a device wall clock. */
typedef struct {
    bool available;
    unsigned earned, eaten, older_pending;
    uint64_t next_tokens, remaining_tokens;
    unsigned progress;
} pet_lunch_t;

pet_lunch_t pet_lunch_read(const pet_life_t *life);
