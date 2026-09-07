#pragma once
#include "pet_house.h"

typedef struct {
    pet_house_t house;
    uint32_t revision;
    uint32_t synced_at;
    bool storage_ok;
    bool ready;
    bool synced_wirelessly;
} pet_snapshot_t;

/* App-lifetime worker owns USB, state and flash; never owns LVGL objects. */
bool pet_service_start(void);
bool pet_service_snapshot(pet_snapshot_t *snapshot);
/* Bind actions to the UI snapshot, not a newer worker state the user never saw. */
bool pet_service_eat(const pet_snapshot_t *shown);
bool pet_service_choose(const pet_snapshot_t *shown, unsigned species_id);
