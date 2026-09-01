#pragma once

#include <stdbool.h>

#include "pet_model.h"

/*
 * NVS persistence for the pet model.
 *
 * Reads happen once when the demo is entered. Writes are copied into a
 * single-element queue and committed by a worker task, so button callbacks
 * never block on flash I/O.
 */
bool pet_store_init(void);
bool pet_store_load(pet_model_t *model);
bool pet_store_request_save(const pet_model_t *model);
