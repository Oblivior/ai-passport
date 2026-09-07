#pragma once
#include <stdbool.h>
#include <stddef.h>

/* App-lifetime radio. Credentials are provisioned over trusted USB only. */
typedef struct {
    bool paired;
    bool ready;
    bool connected;
    bool authenticated;
    int error;
    char id[9];
} pet_ble_status_t;

void pet_ble_init(void); /* Called by the pet worker, not a button callback. */
bool pet_ble_pair(const char *hex_key); /* Same-key retries are idempotent. */
void pet_ble_status(pet_ble_status_t *out);
bool pet_ble_receive(char *line, size_t capacity); /* Authenticated requests only. */
void pet_ble_reply(const char *line); /* Encrypted response to the last request. */
