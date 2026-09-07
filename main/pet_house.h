#pragma once
#include "pet_life.h"
#include "pet_catalog.h"

#define PET_HOUSE_VERSION 5U
typedef struct {
    uint8_t adopted;
    uint8_t adopted_day;
    uint8_t highest_plus_one; /* Lifetime discovery, zero means never adopted. */
    uint8_t eaten[PET_LIFE_DAYS]; /* Allocation of shared food by source date. */
} pet_partner_t;
typedef struct {
    pet_archive_entry_t result;
    uint8_t species_id; /* Zero preserves the original robot demo artwork. */
    uint8_t branch; /* 0 standard; 1 Agumon dark, only for stages 5/6. */
} pet_house_archive_t;
enum { PET_MONTH_LEGACY, PET_MONTH_PENDING, PET_MONTH_SETTLED };
typedef struct {
    uint64_t daily_goal; /* Frozen at rollover, never today's goal. */
    uint64_t official_tokens; /* High-water mark; corrections cannot regress. */
    uint16_t month_meals; /* All partners, including subsequently evicted rows. */
    uint8_t care_days;
    uint8_t chosen_branch;
    uint8_t status;
    uint8_t reserved[3];
} pet_month_record_t;
typedef struct {
    uint32_t version;
    pet_life_t life; /* Shared pantry only. Its stage is NOT a partner stage. */
    pet_partner_t partners[PET_PARTNER_CAPACITY];
    pet_house_archive_t archive[PET_ARCHIVE_MAX];
    uint8_t active_id;
    uint8_t archive_count;
    uint8_t branch_mask; /* Monthly choice, bit (stable species ID - 1). */
    uint8_t branch_seen_mask; /* Lifetime dark forms: bit 0 skull, bit 1 black war. */
    pet_month_record_t months[PET_ARCHIVE_MAX]; /* v5 extension after the v4 prefix. */
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
/* Remove only obsolete species_id=0 demo archives. Does not touch current
 * partners, pantry, lifetime discoveries or real species archives. */
unsigned pet_house_clear_legacy_archives(pet_house_t *house);
unsigned pet_house_branch(const pet_house_t *house, unsigned id);
bool pet_house_branch_choose(pet_house_t *house, unsigned expected_id, uint32_t expected_month,
                             unsigned branch, unsigned bond_points);
bool pet_house_form_seen(const pet_house_t *house, unsigned id, unsigned stage, unsigned branch);
/* Legacy callers must zero the extension before copying the 624-byte prefix. */
bool pet_house_upgrade_v3(pet_house_t *house);
bool pet_house_upgrade_legacy(pet_house_t *house);
/* Only archived, fully covered months. Preserves current food, days and pets. */
bool pet_house_settle(pet_house_t *house, uint32_t month, uint64_t tokens, uint32_t covered_through);
