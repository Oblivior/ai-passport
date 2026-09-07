#include "pet_meet.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    pet_meet_t a, b, c;
    uint8_t frame[PET_MEET_WIRE_SIZE], other[PET_MEET_WIRE_SIZE];
    assert(!pet_meet_start(&a, 0, 0, 1, 3, 0));
    assert(!pet_meet_start(&a, 0, 1, 2, 6, 1));
    assert(!pet_meet_start(&a, 0, 1, 1, 4, 1));
    assert(pet_meet_start(&a, 0, 11, 1, 5, 1));
    assert(pet_meet_start(&b, 0, 22, 2, 4, 0));
    assert(pet_meet_encode(&a, frame) && frame[3] == (1 | 5 << 2 | 1 << 5));
    assert(!pet_meet_receive(&a, frame, sizeof(frame), -40, 0)); /* Self echo. */
    assert(!pet_meet_receive(&b, frame, sizeof(frame)-1, -40, 0));
    assert(!pet_meet_receive(&b, frame, sizeof(frame), -66, 0));
    assert(!pet_meet_receive(&b, frame, sizeof(frame), 127, 0));
    for (unsigned v = 0; v < 256; v++) {
        memcpy(other, frame, sizeof(frame)); other[2] = v;
        if (v != 0xa1) assert(!pet_meet_receive(&b, other, sizeof(other), -40, 0));
    }
    assert(pet_meet_receive(&b, frame, sizeof(frame), -40, 0));
    assert(!pet_meet_confirm(&b, 100)); /* Two temporally separated sightings required. */
    assert(pet_meet_receive(&b, frame, sizeof(frame), -40, 160));
    assert(b.sightings == 2 && !b.complete);
    assert(pet_meet_confirm(&b, 200) && b.confirmed && !b.complete);
    assert(pet_meet_encode(&b, other));
    assert(pet_meet_receive(&a, other, sizeof(other), -40, 210));
    assert(pet_meet_receive(&a, other, sizeof(other), -40, 400));
    assert(a.peer_confirmed && !a.complete); /* Remote consent alone is insufficient. */
    assert(pet_meet_confirm(&a, 410) && a.complete);
    assert(pet_meet_encode(&a, frame));
    assert(pet_meet_receive(&b, frame, sizeof(frame), -40, 420) && b.complete);
    assert(!pet_meet_confirm(&a, 450));
    assert(pet_meet_start(&c, 0, 33, 3, 3, 0));
    assert(!pet_meet_receive(&c, frame, sizeof(frame), -40, 450)); /* ACK for somebody else. */
    assert(pet_meet_start(&a, 500, 44, 1, 3, 0));
    assert(!pet_meet_receive(&a, other, sizeof(other), -40, 510)); /* Old session target. */
    assert(pet_meet_start(&b, 500, 55, 2, 3, 0));
    assert(pet_meet_encode(&b, frame));
    assert(pet_meet_receive(&a, frame, sizeof(frame), -40, 600));
    assert(pet_meet_receive(&a, frame, sizeof(frame), -40, 800));
    assert(pet_meet_confirm(&a, 900));
    pet_meet_tick(&a, 5800); assert(!a.peer && !a.confirmed && a.active);
    pet_meet_tick(&a, 60500); assert(!a.active && !pet_meet_encode(&a, frame));
    assert(pet_meet_start(&a, UINT32_MAX - 100, 77, 1, 0, 0));
    pet_meet_tick(&a, 100); assert(a.active);
    pet_meet_tick(&a, (UINT32_MAX - 100) + 60000U); assert(!a.active);
    assert(!strcmp(pet_meet_greeting(1, 2), pet_meet_greeting(2, 1)));
    puts("pet_meet: PASS (two peers, mutual consent, stale/self/third-party frames, malformed packets, expiry, wrap)");
}
