#pragma once
#include "pet_life.h"

typedef struct {
    pet_life_t life;
    uint32_t revision;
    uint32_t synced_at;
    bool storage_ok;
    bool ready;
} pet_snapshot_t;

/* App-lifetime worker owns USB, state and flash; never owns LVGL objects. */
bool pet_service_start(void);
bool pet_service_snapshot(pet_snapshot_t *snapshot);
bool pet_service_eat(void); /* Queue only: safe in a button callback. */
