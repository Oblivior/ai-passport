#pragma once
#include "pet_catalog.h"

#define PET_MEET_WIRE_SIZE 12U
#define PET_MEET_WINDOW_MS 60000U
typedef struct {
    uint32_t nonce, peer, started_at, seen_at, first_seen_at;
    uint8_t species, stage, branch, peer_species, peer_stage, peer_branch;
    uint8_t sightings;
    bool active, confirmed, peer_confirmed, complete;
} pet_meet_t;
/* Optional, public toy greeting. Nonces prevent accidental cross-session ACKs,
 * not impersonation. Never carries identity, tokens, keys or growth rewards. */
bool pet_meet_start(pet_meet_t *meet, uint32_t now, uint32_t nonce,
                    unsigned species, unsigned stage, unsigned branch);
void pet_meet_tick(pet_meet_t *meet, uint32_t now);
bool pet_meet_confirm(pet_meet_t *meet, uint32_t now);
bool pet_meet_encode(const pet_meet_t *meet, uint8_t out[PET_MEET_WIRE_SIZE]);
bool pet_meet_receive(pet_meet_t *meet, const uint8_t *data, size_t size, int rssi, uint32_t now);
const char *pet_meet_greeting(unsigned a, unsigned b);
