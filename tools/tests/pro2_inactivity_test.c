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
    for (int second = 1; second < PRO2_INACTIVITY_TIMEOUT_US / 1000000LL; ++second) {
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
        assert(!pro2_inactivity_update(&timer, &state, PRO2_INACTIVITY_TIMEOUT_US - 1000000LL));
        /* A held button, including digital triggers, remains active. */
        assert(!pro2_inactivity_update(&timer, &state, 1300000000LL));
        state.buttons = 0;
        assert(!pro2_inactivity_update(&timer, &state, 1300000000LL + PRO2_INACTIVITY_TIMEOUT_US - 1));
        assert(pro2_inactivity_update(&timer, &state, 1300000000LL + PRO2_INACTIVITY_TIMEOUT_US));
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
    ble_idle_wake_t gate = {0};
    assert(ble_idle_wake_ready(&gate, 0));
    ble_idle_wake_arm(&gate);
    assert(!ble_idle_wake_ready(&gate, 600000000LL));
    ble_idle_wake_disconnected(&gate, 600000000LL);
    /* Even continuous advertisements cannot move the absolute deadline. */
    for (int ms = 0; ms < 1000; ++ms) {
        assert(!ble_idle_wake_ready(&gate, 600000000LL + ms * 1000LL));
    }
    assert(!ble_idle_wake_ready(&gate, 600999999LL));
    /* The deadline releases all controllers; no original peer is needed. */
    assert(ble_idle_wake_ready(&gate, 601000000LL));
    assert(!gate.blocked);
    assert(ble_idle_wake_ready(&gate, 601000001LL));
    /* Delayed scanning does not start another waiting period. */
    ble_idle_wake_arm(&gate);
    ble_idle_wake_disconnected(&gate, 700000000LL);
    ble_idle_wake_disconnected(&gate, 700500000LL);
    assert(ble_idle_wake_ready(&gate, 710000000LL));
    usb_source_presence_t usb = {.attached = true};
    assert(!usb_source_presence_wanted(&usb, false, false, true, 0, 700000000LL));
    usb_source_presence_detached(&usb, 700000000LL);
    assert(!usb_source_presence_wanted(&usb, true, false, false, INT64_MAX, 710000000LL));
    assert(usb_source_presence_wanted(&usb, true, true, true, 0, 710000000LL));
    pro2_inactivity_t timer = {0};
    switch2_state_t state = neutral();
    assert(!pro2_inactivity_update(&timer, &state, 710000000LL));
    assert(!pro2_inactivity_update(&timer, &state, 710000000LL + PRO2_INACTIVITY_TIMEOUT_US - 1));
    assert(pro2_inactivity_update(&timer, &state, 710000000LL + PRO2_INACTIVITY_TIMEOUT_US));
}

int main(void)
{
    test_timeout_and_motion();
    test_controls();
    test_disconnect_and_wake();
    puts("Pro2 inactivity and wake tests passed");
    return 0;
}
