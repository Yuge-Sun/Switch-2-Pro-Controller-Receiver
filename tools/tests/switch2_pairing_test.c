#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "switch2_pairing.h"

static const uint8_t host[6] = {0x0a,0x04,0xfe,0xf6,0x1b,0x44};
static const uint8_t peer[6] = {0x1b,0xee,0x6a,0xab,0xa9,0x3c};
/* Published command/challenge vector, independent of implementation. */
static const uint8_t entropy[32] = {
    0x35,0x03,0xe9,0x29,0x82,0x87,0x71,0x24,0xbe,0xa8,0x0c,0x66,0x46,0x15,0x83,0x4b,
    0x6f,0xc6,0xdf,0x8a,0xd8,0xfe,0xdf,0x15,0xbb,0x8c,0x15,0xe9,0x1f,0x32,0x05,0x44
};
static const uint8_t address_reply[] = {0x15,1,1,1,0x10,0x78,0,0,1,4,1,0x1b,0xee,0x6a,0xab,0xa9,0x3c};
static const uint8_t key_reply[] = {0x15,1,1,4,0x10,0x78,0,0,1,
    0x5c,0xf6,0xee,0x79,0x2c,0xdf,0x05,0xe1,0xba,0x2b,0x63,0x25,0xc4,0x1a,0x5f,0x10};
static const uint8_t confirm_reply[] = {0x15,1,1,2,0x10,0x78,0,0,1,
    0x13,0x4c,0x97,0xf5,0x11,0xb9,0xb6,0xdd,0x4d,0x86,0xfd,0x40,0xf5,0x36,0xe9,0xed};
static const uint8_t final_reply[] = {0x15,1,1,3,0x10,0x78,0,0,1};

static switch2_pairing_t at_stage(unsigned step)
{
    switch2_pairing_t p;
    switch2_pairing_begin(&p, host, peer, entropy);
    if (step >= 1) assert(switch2_pairing_reply(&p, address_reply, sizeof(address_reply)) == 1);
    if (step >= 2) assert(switch2_pairing_reply(&p, key_reply, sizeof(key_reply)) == 1);
    if (step >= 3) assert(switch2_pairing_reply(&p, confirm_reply, sizeof(confirm_reply)) == 1);
    return p;
}

int main(void)
{
    uint8_t packet[25];
    switch2_pairing_t p = at_stage(0);
    const uint8_t address_cmd[] = {0x15,0x91,1,1,0,8,0,0,0,1,0x0a,0x04,0xfe,0xf6,0x1b,0x44};
    assert(switch2_pairing_command(&p, packet) == sizeof(address_cmd));
    assert(!memcmp(packet, address_cmd, sizeof(address_cmd)));
    assert(switch2_pairing_reply(&p, final_reply, sizeof(final_reply)) == 0);
    assert(p.stage == SWITCH2_PAIR_ADDRESS); /* premature commit ACK ignored */
    assert(switch2_pairing_reply(&p, address_reply, sizeof(address_reply)) == 1);
    assert(switch2_pairing_command(&p, packet) == 25 && packet[3] == 4);
    assert(!memcmp(packet + 9, entropy, 16));
    assert(switch2_pairing_reply(&p, key_reply, sizeof(key_reply)) == 1);
    assert(switch2_pairing_command(&p, packet) == 25 && packet[3] == 2);
    assert(!memcmp(packet + 9, entropy + 16, 16));
    assert(switch2_pairing_reply(&p, address_reply, sizeof(address_reply)) == 0);
    assert(switch2_pairing_reply(&p, confirm_reply, sizeof(confirm_reply)) == 1);
    assert(switch2_pairing_command(&p, packet) == 9 && packet[3] == 3 && packet[8] == 0);
    assert(switch2_pairing_reply(&p, final_reply, sizeof(final_reply)) == 2);
    assert(p.stage == SWITCH2_PAIR_DONE && switch2_pairing_command(&p, packet) == 0);
    const uint8_t zero[16] = {0};
    assert(!memcmp(p.public_key, zero, 16) && !memcmp(p.challenge, zero, 16));

    const uint8_t *replies[] = {address_reply, key_reply, confirm_reply, final_reply};
    const size_t lengths[] = {sizeof(address_reply), sizeof(key_reply), sizeof(confirm_reply), sizeof(final_reply)};
    for (unsigned step = 0; step < 4; ++step) {
        for (size_t len = 1; len < lengths[step]; ++len) {
            p = at_stage(step);
            assert(switch2_pairing_reply(&p, replies[step], len) == -1);
            assert(p.stage == SWITCH2_PAIR_FAILED && !switch2_pairing_command(&p, packet));
        }
        for (unsigned field = 1; field <= 8; ++field) {
            if (field == 3) continue; /* out-of-order replies ignored, tested above */
            uint8_t bad[25];
            memcpy(bad, replies[step], lengths[step]);
            bad[field] ^= 0xff;
            p = at_stage(step);
            assert(switch2_pairing_reply(&p, bad, lengths[step]) == -1);
        }
        p = at_stage(step);
        switch2_pairing_clear(&p); /* disconnect/reset at every step */
        assert(!switch2_pairing_command(&p, packet));
        assert(switch2_pairing_reply(&p, replies[step], lengths[step]) == 0);
    }
    for (size_t i = 9; i < sizeof(confirm_reply); ++i) {
        uint8_t bad[25];
        memcpy(bad, confirm_reply, sizeof(bad));
        bad[i] ^= 1;
        p = at_stage(2);
        assert(switch2_pairing_reply(&p, bad, sizeof(bad)) == -1);
        assert(!switch2_pairing_command(&p, packet)); /* never finalize bad proof */
    }
    uint8_t wrong_peer[sizeof(address_reply)];
    memcpy(wrong_peer, address_reply, sizeof(wrong_peer));
    wrong_peer[11] ^= 1;
    p = at_stage(0);
    assert(switch2_pairing_reply(&p, wrong_peer, sizeof(wrong_peer)) == -1);
    uint8_t adv[26] = {0x53,5,1,0,3,0x7e,5,0x69,0x20,0,1,0};
    assert(switch2_pairing_advertisement(adv, sizeof(adv)));
    for (size_t i = 0; i < 18; ++i) assert(!switch2_pairing_advertisement(adv, i));
    memcpy(adv + 12, host, 6);
    assert(!switch2_pairing_advertisement(adv, sizeof(adv))); /* normal reconnect */
    memset(adv + 12, 0, 6);
    adv[7] = 0x66;
    assert(!switch2_pairing_advertisement(adv, sizeof(adv))); /* different device */
    puts("Switch 2 pairing transcript, AES vector, malformed replies, reset and advertisement tests passed");
}
