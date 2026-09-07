#pragma once

#include "pet_model.h"

/* Independent from the legacy demo model: v1 saves are never overwritten. */
#define PET_LIFE_VERSION 2U
#define PET_LIFE_DAYS 31U

typedef struct {
    uint32_t version;
    uint32_t date;                 /* YYYYMMDD, supplied by the companion. */
    uint8_t adopted_day;
    uint8_t earned[PET_LIFE_DAYS];  /* Cumulative daily entitlement, not deltas. */
    uint8_t eaten[PET_LIFE_DAYS];
    uint8_t stage;
    uint16_t legacy_mask;          /* Archive rows imported from the old demo. */
    uint64_t tokens_today;
    uint64_t daily_goal;           /* Locked for the month. */
    pet_model_t family;
} pet_life_t;

typedef struct {
    uint32_t date;
    uint64_t tokens_today;
    uint64_t daily_goal;
    uint8_t earned[PET_LIFE_DAYS];
} pet_usage_t;

bool pet_life_date_valid(uint32_t date);
void pet_life_init(pet_life_t *life, const pet_model_t *legacy);
bool pet_life_valid(const pet_life_t *life);
bool pet_life_sync(pet_life_t *life, const pet_usage_t *usage);
bool pet_life_eat(pet_life_t *life);
unsigned pet_life_pending(const pet_life_t *life);
unsigned pet_life_meals(const pet_life_t *life);
unsigned pet_life_days(const pet_life_t *life);
unsigned pet_life_next_meals(const pet_life_t *life);
unsigned pet_life_next_days(const pet_life_t *life);

/* Daily food tiers describe usage intensity, not quality or tool diversity.
 * Completed days only: today's growing entitlement must not bias the lock.
 * The existing family.route field stores the current month's locked route;
 * this does not change the v2 on-flash structure or namespace. */
pet_route_t pet_life_route(const pet_life_t *life);
bool pet_life_route_locked(const pet_life_t *life);
const char *pet_life_route_hint(pet_route_t route);
