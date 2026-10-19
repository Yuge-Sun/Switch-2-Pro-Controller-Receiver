#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    SWITCH2_PAIR_IDLE, SWITCH2_PAIR_ADDRESS, SWITCH2_PAIR_KEY,
    SWITCH2_PAIR_CONFIRM, SWITCH2_PAIR_FINALIZE, SWITCH2_PAIR_DONE,
    SWITCH2_PAIR_FAILED
} switch2_pair_stage_t;

typedef struct {
    switch2_pair_stage_t stage;
    uint8_t host[6], peer[6], public_key[16], challenge[16], expected[16];
} switch2_pairing_t;

void switch2_pairing_clear(switch2_pairing_t *pair);
void switch2_pairing_begin(switch2_pairing_t *pair, const uint8_t host[6],
                          const uint8_t peer[6], const uint8_t entropy[32]);
size_t switch2_pairing_command(const switch2_pairing_t *pair, uint8_t out[25]);
/* 0: unrelated reply; 1: next step; 2: completed; -1: invalid reply. */
int switch2_pairing_reply(switch2_pairing_t *pair, const uint8_t *data, size_t len);
bool switch2_pairing_advertisement(const uint8_t *mfg, size_t len);
