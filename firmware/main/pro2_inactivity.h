#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "switch2_state.h"

#define PRO2_INACTIVITY_TIMEOUT_US (10LL * 60LL * 1000000LL)
/* About 8% of travel from centre; ignore ordinary stick centre noise.
 * This affects only inactivity detection, never the reports sent to games.
 */
#define PRO2_ACTIVITY_STICK_DEADZONE 160

typedef struct {
    bool started;
    bool expired;
    int64_t last_activity_us;
} pro2_inactivity_t;

static inline bool pro2_axis_active(uint16_t axis)
{
    return axis < 2048 - PRO2_ACTIVITY_STICK_DEADZONE ||
           axis > 2048 + PRO2_ACTIVITY_STICK_DEADZONE;
}

static inline bool pro2_controls_active(const switch2_state_t *state)
{
    /* Motion, including gyro and accelerometer, is deliberately ignored.
     * Held buttons/triggers or a held deflected stick count as input.
     */
    return state->buttons != 0 ||
           pro2_axis_active(state->lx) || pro2_axis_active(state->ly) ||
           pro2_axis_active(state->rx) || pro2_axis_active(state->ry);
}

/* Call from the input task with fresh, warmed-up reports only. */
static inline bool pro2_inactivity_update(pro2_inactivity_t *timer,
                                          const switch2_state_t *state,
                                          int64_t now_us)
{
    if (!timer->started || pro2_controls_active(state)) {
        timer->started = true;
        timer->expired = false;
        timer->last_activity_us = now_us;
    } else if (now_us - timer->last_activity_us >= PRO2_INACTIVITY_TIMEOUT_US) {
        timer->expired = true;
    }
    return timer->expired;
}

