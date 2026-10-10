#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Owned exclusively by the input task; USB suspend is not a source disconnect. */
#define DS5_USB_SOURCE_TIMEOUT_US 1000000LL
#define DS5_USB_DETACH_MIN_US 250000LL

typedef struct {
    bool attached;
    bool detached_once;
    int64_t detached_us;
} usb_source_presence_t;

static inline void usb_source_presence_detached(usb_source_presence_t *presence,
                                                int64_t now_us)
{
    presence->attached = false;
    presence->detached_once = true;
    presence->detached_us = now_us;
}

static inline bool usb_source_presence_wanted(const usb_source_presence_t *presence,
                                              bool connected,
                                              bool warmed_up,
                                              bool live,
                                              int64_t age_us,
                                              int64_t now_us)
{
    if (!connected || !live || age_us < 0 ||
        age_us >= DS5_USB_SOURCE_TIMEOUT_US) {
        return false;
    }
    if (presence->attached) {
        /* Short notification gaps get neutral reports, not USB hotplug churn. */
        return true;
    }
    return warmed_up &&
           (!presence->detached_once ||
            now_us - presence->detached_us >= DS5_USB_DETACH_MIN_US);
}

