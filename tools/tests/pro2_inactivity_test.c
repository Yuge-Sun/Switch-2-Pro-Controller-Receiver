#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pro2_inactivity.h"
#include "ble_idle_wake.h"
#include "usb_source_presence.h"

static switch2_state_t neutral(void)
{
    return (switch2_state_t){.lx = 2048, .ly = 2048, .rx = 2048, .ry = 2048};
}

static void test_timeout_and_motion(void)
{
    pro2_inactivity_t timer = {0};
    switch2_state_t state = neutral();
    assert(!pro2_inactivity_update(&timer, &state, 0));
    /* Live, changing motion must not restart the clock. */
    for (int second = 1; second < 600; ++second) {
        state.motion_valid = true;
        memset(state.motion, second & 255, sizeof(state.motion));
        state.lx = 2048 + (second % 161);
        state.ry = 2048 - (second % 161);
        assert(!pro2_inactivity_update(&timer, &state, second * 1000000LL));
    }
    assert(!pro2_inactivity_update(&timer, &state, PRO2_INACTIVITY_TIMEOUT_US - 1));
    assert(pro2_inactivity_update(&timer, &state, PRO2_INACTIVITY_TIMEOUT_US));
    assert(pro2_inactivity_update(&timer, &state, PRO2_INACTIVITY_TIMEOUT_US + 1));
}

static void test_controls(void)
{
    for (unsigned button = 0; button < SWITCH2_BUTTON_COUNT; ++button) {
        pro2_inactivity_t timer = {0};
        switch2_state_t state = neutral();
        assert(!pro2_inactivity_update(&timer, &state, 0));
        state.buttons = 1u << button;
        assert(!pro2_inactivity_update(&timer, &state, 599000000LL));
        /* A held button, including digital triggers, remains active. */
        assert(!pro2_inactivity_update(&timer, &state, 1300000000LL));
        state.buttons = 0;
        assert(!pro2_inactivity_update(&timer, &state, 1899999999LL));
        assert(pro2_inactivity_update(&timer, &state, 1900000000LL));
    }
    switch2_state_t state = neutral();
    uint16_t *axes[] = {&state.lx, &state.ly, &state.rx, &state.ry};
    for (unsigned i = 0; i < 4; ++i) {
        *axes[i] = 2048 - PRO2_ACTIVITY_STICK_DEADZONE;
        assert(!pro2_controls_active(&state));
        --*axes[i];
        assert(pro2_controls_active(&state));
        *axes[i] = 2048 + PRO2_ACTIVITY_STICK_DEADZONE;
        assert(!pro2_controls_active(&state));
        ++*axes[i];
        assert(pro2_controls_active(&state));
        *axes[i] = 2048;
    }
    pro2_inactivity_t timer = {0};
    state.rx = 4095;
    assert(!pro2_inactivity_update(&timer, &state, 0));
    assert(!pro2_inactivity_update(&timer, &state, 2 * PRO2_INACTIVITY_TIMEOUT_US));
    state.rx = 2048;
    /* Missing/unstable reports simply do not update the inactivity policy. */
    assert(pro2_inactivity_update(&timer, &state, 3 * PRO2_INACTIVITY_TIMEOUT_US));
}

static void test_disconnect_and_wake(void)
{
    ble_idle_wake_t gate = {.blocked = true};
    usb_source_presence_t usb = {.attached = true};
    assert(!ble_idle_wake_advertisement(&gate, true, 600000000LL));
    ble_idle_wake_scan_started(&gate, 600000000LL);
    /* Persistent advertising after termination must not auto-reconnect. */
    for (int second = 600; second < 1200; ++second) {
        assert(!ble_idle_wake_advertisement(&gate, true, second * 1000000LL));
    }
    assert(!usb_source_presence_wanted(&usb, false, false, true, 0, 1200000000LL));
    usb_source_presence_detached(&usb, 1200000000LL);
    assert(!ble_idle_wake_advertisement(&gate, false, 1300000000LL));
    /* Turning off, then on after a quiet period, permits the saved peer. */
    assert(ble_idle_wake_advertisement(&gate, true, 1300000000LL));
    assert(!gate.blocked);
    assert(!usb_source_presence_wanted(&usb, true, false, false, INT64_MAX, 1300000000LL));
    assert(usb_source_presence_wanted(&usb, true, true, true, 0, 1300000000LL));
    pro2_inactivity_t new_connection = {0};
    switch2_state_t state = neutral();
    assert(!pro2_inactivity_update(&new_connection, &state, 1300000000LL));
    assert(!pro2_inactivity_update(&new_connection, &state, 1899999999LL));
    assert(pro2_inactivity_update(&new_connection, &state, 1900000000LL));

    gate = (ble_idle_wake_t){.blocked = true};
    ble_idle_wake_scan_started(&gate, 0);
    assert(!ble_idle_wake_advertisement(&gate, true, BLE_IDLE_WAKE_QUIET_US - 1));
    gate.observing = false; /* Scan interrupted, e.g. by UART management. */
    assert(!ble_idle_wake_advertisement(&gate, true, 100000000LL));
    ble_idle_wake_scan_started(&gate, 100000000LL);
    assert(!ble_idle_wake_advertisement(&gate, true, 100000000LL));
    assert(ble_idle_wake_advertisement(&gate, true, 100000000LL + BLE_IDLE_WAKE_QUIET_US));
}

int main(void)
{
    test_timeout_and_motion();
    test_controls();
    test_disconnect_and_wake();
    puts("Pro2 inactivity and wake tests passed");
    return 0;
}
