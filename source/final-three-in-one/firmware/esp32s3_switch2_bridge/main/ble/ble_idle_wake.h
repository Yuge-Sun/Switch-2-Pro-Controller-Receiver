#pragma once
#include <stdbool.h>
#include <stdint.h>

#define BLE_IDLE_WAKE_QUIET_US 5000000LL

typedef struct {
    bool blocked;
    bool observing;
    int64_t last_seen_us;
} ble_idle_wake_t;

/* A fresh continuous scan must observe silence itself. Time with the radio
 * stopped (or duplicate filtering enabled) is not evidence of controller sleep.
 * Callers serialize this policy across the input task and NimBLE callbacks.
 */
static inline void ble_idle_wake_scan_started(ble_idle_wake_t *gate, int64_t now_us)
{
    gate->observing = true;
    gate->last_seen_us = now_us;
}

static inline bool ble_idle_wake_advertisement(ble_idle_wake_t *gate,
                                              bool same_peer, int64_t now_us)
{
    if (!gate->blocked) {
        return true;
    }
    if (!same_peer || !gate->observing) {
        return false;
    }
    bool woke = now_us - gate->last_seen_us >= BLE_IDLE_WAKE_QUIET_US;
    gate->last_seen_us = now_us;
    if (woke) {
        gate->blocked = false;
    }
    return woke;
}

