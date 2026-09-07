#include "pet_meet.h"
#include <string.h>

static bool form_valid(unsigned id, unsigned stage, unsigned branch)
{
    return id <= 3 && pet_catalog_find(id) && stage < PET_STAGE_COUNT && branch <= 1 &&
        (!branch || (pet_catalog_branch_supported(id) && stage >= PET_STAGE_TITAN));
}
static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
static void put32(uint8_t *p, uint32_t n)
{
    p[0] = n >> 24; p[1] = n >> 16; p[2] = n >> 8; p[3] = n;
}
bool pet_meet_start(pet_meet_t *m, uint32_t now, uint32_t nonce, unsigned species, unsigned stage, unsigned branch)
{
    if (!m || !nonce || !form_valid(species, stage, branch)) return false;
    *m = (pet_meet_t){.nonce = nonce, .started_at = now, .species = species,
        .stage = stage, .branch = branch, .active = true};
    return true;
}
void pet_meet_tick(pet_meet_t *m, uint32_t now)
{
    if (!m || !m->active) return;
    if (now - m->started_at >= PET_MEET_WINDOW_MS) { m->active = false; return; }
    if (m->peer && !m->complete && now - m->seen_at >= 5000U) {
        m->peer = 0; m->peer_species = m->peer_stage = m->peer_branch = m->sightings = 0;
        m->confirmed = m->peer_confirmed = false;
    }
}
bool pet_meet_confirm(pet_meet_t *m, uint32_t now)
{
    pet_meet_tick(m, now);
    if (!m || !m->active || !m->peer || m->sightings < 2 || m->complete) return false;
    m->confirmed = true;
    m->complete = m->peer_confirmed;
    return true;
}
bool pet_meet_encode(const pet_meet_t *m, uint8_t out[PET_MEET_WIRE_SIZE])
{
    if (!m || !out || !m->active || !m->nonce || !form_valid(m->species, m->stage, m->branch)) return false;
    /* 0xffff is an experimental manufacturer marker, not a company identity.
     * 12-byte payload + AD framing + 14-byte local name fits a 31-byte scan response. */
    out[0] = out[1] = 0xff; out[2] = 0xa1;
    out[3] = m->species | m->stage << 2 | m->branch << 5;
    put32(out + 4, m->nonce); put32(out + 8, m->confirmed ? m->peer : 0);
    return true;
}
bool pet_meet_receive(pet_meet_t *m, const uint8_t *p, size_t size, int rssi, uint32_t now)
{
    pet_meet_tick(m, now);
    if (!m || !m->active || m->complete || !p || size != PET_MEET_WIRE_SIZE || rssi < -65 || rssi > 0 ||
        p[0] != 0xff || p[1] != 0xff || p[2] != 0xa1 || (p[3] & 0xc0)) return false;
    unsigned species = p[3] & 3, stage = (p[3] >> 2) & 7, branch = p[3] >> 5;
    uint32_t peer = get32(p + 4), target = get32(p + 8);
    if (!peer || peer == m->nonce || !form_valid(species, stage, branch) ||
        (target && target != m->nonce) || (m->peer && m->peer != peer)) return false;
    if (!m->peer) {
        m->peer = peer; m->first_seen_at = now; m->sightings = 1;
        m->peer_species = species; m->peer_stage = stage; m->peer_branch = branch;
    } else if (m->peer_species != species || m->peer_stage != stage || m->peer_branch != branch) return false;
    else if (now - m->first_seen_at >= 150U) m->sightings = 2;
    m->seen_at = now;
    m->peer_confirmed = target == m->nonce && m->sightings >= 2;
    m->complete = m->confirmed && m->peer_confirmed;
    return true;
}
const char *pet_meet_greeting(unsigned a, unsigned b)
{
    if (a == b) return "遇到同族啦！";
    if ((a == PET_AGUMON && b == PET_GABUMON) || (b == PET_AGUMON && a == PET_GABUMON)) return "勇气与友情，集合！";
    return "新的冒险搭档！";
}
