#pragma once
#include "pet_house.h"
#include "pet_bond.h"

typedef enum { PET_TRAIN_NONE, PET_TRAIN_REWARDED, PET_TRAIN_PRACTICE,
    PET_TRAIN_OFFLINE, PET_TRAIN_STALE, PET_TRAIN_SAVE_ERROR } pet_train_result_t;

typedef struct {
    pet_house_t house;
    pet_bond_t bond;
    uint32_t training_ticket;
    pet_train_result_t training_result;
    uint8_t training_gain;
    bool bond_storage_ok;
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
/* Local UI completion only; no USB/BLE mutation command. Nonzero increasing
 * tickets prevent repeated result clicks from awarding the same game twice. */
bool pet_service_train(const pet_snapshot_t *shown, unsigned score, unsigned attempts, uint32_t ticket);
