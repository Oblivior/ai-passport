#pragma once
#include "pet_model.h"

/* Persistent IDs: never renumber/reuse these when the visible list changes. */
typedef enum { PET_SPECIES_NONE = 0, PET_AGUMON = 1, PET_GABUMON = 2, PET_PATAMON = 3 } pet_species_t;
#define PET_PARTNER_CAPACITY 8U
#define PET_CATALOG_COUNT 3U
typedef struct {
    uint8_t id;
    uint8_t artwork;
    const char *name;
    const char *forms[PET_STAGE_COUNT];
} pet_species_info_t;
const pet_species_info_t *pet_catalog_at(unsigned index);
const pet_species_info_t *pet_catalog_find(unsigned id);
const char *pet_catalog_form(unsigned id, unsigned stage);
unsigned pet_catalog_meals(unsigned stage);
unsigned pet_catalog_days(unsigned stage);
