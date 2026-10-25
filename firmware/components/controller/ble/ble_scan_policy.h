#pragma once
#include <stdbool.h>
#include <stdint.h>

/* BLE scan interval/window units are 0.625 ms. Keep the controller running
 * throughout periodic scanning so it schedules the radio windows itself. */
#define BLE_SCAN_FAST_US 60000000LL
#define BLE_SCAN_RETRY_US 3000000LL
#define BLE_SCAN_IDLE_INTERVAL_UNITS 7200 /* 4500 ms */
#define BLE_SCAN_IDLE_WINDOW_UNITS 160    /*  100 ms */
#define BLE_SCAN_FAST_UNITS 48            /*   30 ms */

typedef struct {
    int64_t fast_until_us;
    int64_t retry_until_us;
} ble_scan_policy_t;

typedef struct {
    bool passive;
    uint16_t interval;
    uint16_t window;
    int32_t duration_ms;
} ble_scan_plan_t;

/* Only a host sync or a real link loss starts a new one-minute episode.
 * Scan restarts, watchdog checks and failed attempts must not call this. */
static inline void ble_scan_policy_begin(ble_scan_policy_t *policy,
                                         int64_t now_us, int64_t cooldown_us)
{
    policy->fast_until_us = now_us + cooldown_us + BLE_SCAN_FAST_US;
    policy->retry_until_us = 0;
}

static inline int64_t ble_scan_policy_deadline(const ble_scan_policy_t *policy)
{
    return policy->fast_until_us > policy->retry_until_us ?
        policy->fast_until_us : policy->retry_until_us;
}

/* A fresh detection from periodic scanning permits one short recovery burst.
 * Detections during that burst cannot keep extending it. */
static inline void ble_scan_policy_detected(ble_scan_policy_t *policy, int64_t now_us)
{
    if (now_us >= ble_scan_policy_deadline(policy)) policy->retry_until_us = 0;
}

static inline void ble_scan_policy_failed(ble_scan_policy_t *policy, int64_t now_us)
{
    if (policy->retry_until_us == 0) policy->retry_until_us = now_us + BLE_SCAN_RETRY_US;
}

static inline ble_scan_plan_t ble_scan_policy_plan(const ble_scan_policy_t *policy,
                                                  int64_t now_us)
{
    int64_t remaining_us = ble_scan_policy_deadline(policy) - now_us;
    if (remaining_us > 0) {
        return (ble_scan_plan_t){.passive = false, .interval = BLE_SCAN_FAST_UNITS,
            .window = BLE_SCAN_FAST_UNITS, .duration_ms = (int32_t)((remaining_us + 999) / 1000)};
    }
    return (ble_scan_plan_t){.passive = true, .interval = BLE_SCAN_IDLE_INTERVAL_UNITS,
        .window = BLE_SCAN_IDLE_WINDOW_UNITS, .duration_ms = INT32_MAX};
}
