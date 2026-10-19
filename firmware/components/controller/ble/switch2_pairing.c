#include "switch2_pairing.h"
#include <string.h>
#include "mbedtls/aes.h"
#include "mbedtls/platform_util.h"

void switch2_pairing_clear(switch2_pairing_t *p)
{
    mbedtls_platform_zeroize(p, sizeof(*p));
}

bool switch2_pairing_advertisement(const uint8_t *mfg, size_t len)
{
    static const uint8_t prefix[] = {0x53, 0x05, 0x01, 0x00, 0x03, 0x7e, 0x05, 0x69, 0x20};
    static const uint8_t zero[6] = {0};
    return mfg && len >= 18 && !memcmp(mfg, prefix, sizeof(prefix)) &&
        mfg[11] == 0 && !memcmp(mfg + 12, zero, sizeof(zero));
}

void switch2_pairing_begin(switch2_pairing_t *p, const uint8_t host[6],
                          const uint8_t peer[6], const uint8_t entropy[32])
{
    switch2_pairing_clear(p);
    p->stage = SWITCH2_PAIR_ADDRESS;
    memcpy(p->host, host, 6);
    memcpy(p->peer, peer, 6);
    memcpy(p->public_key, entropy, 16);
    memcpy(p->challenge, entropy + 16, 16);
}

size_t switch2_pairing_command(const switch2_pairing_t *p, uint8_t out[25])
{
    memset(out, 0, 25);
    out[0] = 0x15;
    out[1] = 0x91;
    out[2] = 0x01;
    switch (p->stage) {
    case SWITCH2_PAIR_ADDRESS:
        out[3] = 1;
        out[5] = 8;
        out[9] = 1; /* One physical Bluetooth adapter, in NimBLE/wire byte order. */
        memcpy(out + 10, p->host, 6);
        return 16;
    case SWITCH2_PAIR_KEY:
        out[3] = 4;
        out[5] = 17;
        memcpy(out + 9, p->public_key, 16);
        return 25;
    case SWITCH2_PAIR_CONFIRM:
        out[3] = 2;
        out[5] = 17;
        memcpy(out + 9, p->challenge, 16);
        return 25;
    case SWITCH2_PAIR_FINALIZE:
        out[3] = 3;
        out[5] = 1;
        return 9;
    default:
        return 0;
    }
}

int switch2_pairing_reply(switch2_pairing_t *p, const uint8_t *d, size_t len)
{
    uint8_t command[25];
    if (!switch2_pairing_command(p, command) || !d || !len || d[0] != 0x15) {
        return 0;
    }
    /* Ignore delayed replies from other pairing steps. A truncated pairing
     * header, or a malformed reply to this step, must never authorize commit. */
    if (len >= 4 && d[3] != command[3]) {
        return 0;
    }
    size_t required = p->stage == SWITCH2_PAIR_ADDRESS ? 17 :
        p->stage == SWITCH2_PAIR_FINALIZE ? 9 : 25;
    if (len != required || d[1] != 1 || d[2] != 1 || d[4] != 0x10 ||
        d[5] != 0x78 || d[6] || d[7] || d[8] != 1) {
        goto fail;
    }
    switch (p->stage) {
    case SWITCH2_PAIR_ADDRESS:
        if (d[10] != 1 || memcmp(d + 11, p->peer, 6)) {
            goto fail;
        }
        p->stage = SWITCH2_PAIR_KEY;
        break;
    case SWITCH2_PAIR_KEY: {
        uint8_t key[16], plain[16];
        for (size_t i = 0; i < 16; ++i) {
            key[i] = p->public_key[15 - i] ^ d[9 + 15 - i];
            plain[i] = p->challenge[15 - i];
        }
        mbedtls_aes_context aes;
        mbedtls_aes_init(&aes);
        int rc = mbedtls_aes_setkey_enc(&aes, key, 128);
        if (!rc) {
            /* Match the published 13 4c ... e9 ed reply vector: reverse key
             * and challenge before AES, but compare ciphertext as returned. */
            rc = mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, plain, p->expected);
        }
        mbedtls_aes_free(&aes);
        mbedtls_platform_zeroize(key, sizeof(key));
        mbedtls_platform_zeroize(plain, sizeof(plain));
        mbedtls_platform_zeroize(p->public_key, sizeof(p->public_key));
        if (rc) {
            goto fail;
        }
        p->stage = SWITCH2_PAIR_CONFIRM;
        break;
    }
    case SWITCH2_PAIR_CONFIRM: {
        uint8_t mismatch = 0;
        for (size_t i = 0; i < 16; ++i) {
            mismatch |= d[9 + i] ^ p->expected[i];
        }
        if (mismatch) {
            goto fail;
        }
        mbedtls_platform_zeroize(p->expected, sizeof(p->expected));
        mbedtls_platform_zeroize(p->challenge, sizeof(p->challenge));
        p->stage = SWITCH2_PAIR_FINALIZE;
        break;
    }
    case SWITCH2_PAIR_FINALIZE:
        switch2_pairing_clear(p);
        p->stage = SWITCH2_PAIR_DONE;
        return 2;
    default:
        goto fail;
    }
    return 1;
fail:
    switch2_pairing_clear(p);
    p->stage = SWITCH2_PAIR_FAILED;
    return -1;
}
