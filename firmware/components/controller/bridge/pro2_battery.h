#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Native Pro2 levels are 0..9, not exact state-of-charge percentages.
#define PRO2_BATTERY_FRESH_US 300000000LL
#define PRO2_BATTERY_POWER_FRESH_US 5000000LL

typedef struct {
    uint8_t level;
    bool level_valid;
    bool level_stale; // Age is separate from whether a confirmed level exists.
    bool power_valid;
    bool charging;
    bool external_power;
    uint16_t voltage_mv;
    int64_t level_us;
    int64_t power_us;
} pro2_battery_t;

static inline bool pro2_battery_decode_primary(const uint8_t *data, size_t len,
                                               int64_t now, pro2_battery_t *out)
{
    if (!data || !out || len < 11 || ((data[1] >> 2) & 15) > 9) return false;
    bool nonzero = false;
    for (size_t i = 0; i < len; ++i) nonzero |= data[i] != 0;
    if (!nonzero) return false; // An all-zero cached report is not an empty battery.
    *out = (pro2_battery_t){
        .level = (data[1] >> 2) & 15, .level_valid = true,
        .power_valid = true, .charging = (data[1] & 2) != 0,
        .external_power = (data[1] & 1) != 0, .level_us = now, .power_us = now,
    };
    return true;
}

static inline void pro2_battery_decode_fd2(const uint8_t *data, size_t len,
                                          int64_t now, pro2_battery_t *out)
{
    if (!data || !out || len < 60) return;
    out->voltage_mv = data[0x1f] | ((uint16_t)data[0x20] << 8);
    uint8_t code = data[0x21]; // 0x22 is NOT the high byte of charge state.
    out->power_valid = code == 0 || code == 0x20 || (code >= 0x30 && code <= 0x3f);
    out->charging = out->power_valid && code >= 0x30;
    out->external_power = out->power_valid && code != 0;
    out->power_us = now;
}

static inline void pro2_battery_expire(pro2_battery_t *battery, int64_t now)
{
    battery->level_stale = now < battery->level_us ||
        now - battery->level_us > PRO2_BATTERY_FRESH_US;
    if (now < battery->power_us || now - battery->power_us > PRO2_BATTERY_POWER_FRESH_US)
        battery->power_valid = false;
}

static inline uint8_t pro2_battery_percent(const pro2_battery_t *battery)
{
    return battery->level_valid && battery->level <= 9
        ? (uint8_t)((battery->level * 100u + 4u) / 9u) : 255;
}

// DualSense payload byte 52: high nibble = power status, low = 10% bucket.
// Preserve the last confirmed level within this connection, even when stale.
// Unknown power state falls back to discharging, never erasing a known level.
static inline uint8_t pro2_battery_dualsense(const pro2_battery_t *battery)
{
    if (!battery->level_valid || battery->level > 9) return 0xf0;
    uint8_t level = (uint8_t)((battery->level * 10u + 4u) / 9u);
    uint8_t status = !battery->power_valid ? 0 : battery->charging ? 1 :
        (battery->external_power && battery->level == 9 ? 2 : 0);
    return (uint8_t)((status << 4) | level);
}
