#pragma once
#include "pet_life.h"
#include "pet_catalog.h"

#define PET_HOUSE_VERSION 3U
typedef struct {
    uint8_t adopted;
    uint8_t adopted_day;
    uint8_t highest_plus_one; /* Lifetime discovery, zero means never adopted. */
    uint8_t eaten[PET_LIFE_DAYS]; /* Allocation of shared food by source date. */
} pet_partner_t;
typedef struct {
    pet_archive_entry_t result;
    uint8_t species_id; /* Zero preserves the original robot demo artwork. */
    uint8_t reserved;
} pet_house_archive_t;
typedef struct {
    uint32_t version;
    pet_life_t life; /* Shared pantry only. Its stage is NOT a partner stage. */
    pet_partner_t partners[PET_PARTNER_CAPACITY];
    pet_house_archive_t archive[PET_ARCHIVE_MAX];
    uint8_t active_id;
    uint8_t archive_count;
    uint8_t reserved[2];
} pet_house_t;

void pet_house_init(pet_house_t *house, const pet_life_t *legacy);
bool pet_house_valid(const pet_house_t *house);
bool pet_house_sync(pet_house_t *house, const pet_usage_t *usage);
bool pet_house_choose(pet_house_t *house, unsigned id);
bool pet_house_eat(pet_house_t *house);
bool pet_house_action(pet_house_t *house, unsigned expected_id, uint32_t expected_month, unsigned choose_id);
const pet_partner_t *pet_house_partner(const pet_house_t *house, unsigned id);
unsigned pet_house_meals(const pet_house_t *house, unsigned id);
unsigned pet_house_days(const pet_house_t *house, unsigned id);
unsigned pet_house_stage(const pet_house_t *house, unsigned id);
