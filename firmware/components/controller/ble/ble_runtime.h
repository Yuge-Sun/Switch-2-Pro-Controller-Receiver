#pragma once

typedef enum {
    BLE_STATE_IDLE = 0,
    BLE_STATE_SCANNING,
    BLE_STATE_CONNECTING,
    BLE_STATE_CONNECTED,
} ble_state_t;

/* Task-context APIs. Initialize before starting the NimBLE host or workers. */
void ble_runtime_init(void);
ble_state_t ble_runtime_state(void);
void ble_runtime_set_state(ble_state_t state);
/* Broadcast, level-triggered wait: a connection before the wait is not lost.
 * Callers must recheck state after waking (the link may already have dropped).
 */
void ble_runtime_wait_connected(void);
