#pragma once
#include <stdbool.h>
#include <stdint.h>

#define BLE_IDLE_WAKE_DELAY_US 1000000LL

typedef struct {
    bool blocked;
    int64_t resume_after_us;
} ble_idle_wake_t;

/* Arm before requesting termination. Start the absolute clock only when the
 * disconnect event arrives. Callers serialize access across tasks. */
static inline void ble_idle_wake_arm(ble_idle_wake_t *gate)
{
    *gate = (ble_idle_wake_t){.blocked = true, .resume_after_us = INT64_MAX};
}

static inline void ble_idle_wake_disconnected(ble_idle_wake_t *gate, int64_t now_us)
{
    if (gate->blocked && gate->resume_after_us == INT64_MAX) {
        gate->resume_after_us = now_us + BLE_IDLE_WAKE_DELAY_US;
    }
}

/* Readiness depends only on elapsed time, never a controller address. */
static inline bool ble_idle_wake_ready(ble_idle_wake_t *gate, int64_t now_us)
{
    if (!gate->blocked) {
        return true;
    }
    if (now_us < gate->resume_after_us) {
        return false;
    }
    gate->blocked = false;
    return true;
}
