#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include "usb_source_presence.h"

int main(void)
{
    usb_source_presence_t usb = {0};

    /* Plugged in without pairing, or bonded but asleep: never enumerate. */
    assert(!usb_source_presence_wanted(&usb, false, false, false, INT64_MAX, 0));
    assert(!usb_source_presence_wanted(&usb, false, false, false, INT64_MAX, 60000000));
    /* BLE connection/security alone is insufficient; require input warmup. */
    assert(!usb_source_presence_wanted(&usb, true, false, false, INT64_MAX, 60000000));
    assert(!usb_source_presence_wanted(&usb, true, false, true, 0, 60000000));
    assert(usb_source_presence_wanted(&usb, true, true, true, 0, 60000000));
    usb.attached = true;

    /* Packet jitter can reset report warmup without hot-unplugging USB. */
    assert(usb_source_presence_wanted(&usb, true, false, true, 250000, 60250000));
    assert(usb_source_presence_wanted(&usb, true, false, true, 999999, 60999999));
    assert(!usb_source_presence_wanted(&usb, true, false, true, 1000000, 61000000));
    assert(!usb_source_presence_wanted(&usb, true, false, false, INT64_MAX, 61000000));
    /* An explicit disconnect wins even when the last input is fresh. */
    assert(!usb_source_presence_wanted(&usb, false, true, true, 0, 61000000));
    usb_source_presence_detached(&usb, 61000000);
    assert(!usb.attached);

    /* Rapid off/on must still produce a host-observable USB detach. */
    assert(!usb_source_presence_wanted(&usb, true, true, true, 0, 61249999));
    assert(usb_source_presence_wanted(&usb, true, true, true, 0, 61250000));
    assert(!usb_source_presence_wanted(&usb, true, false, true, 0, 61250000));
    assert(!usb_source_presence_wanted(&usb, true, true, true, -1, 61250000));
    assert(!usb_source_presence_wanted(&usb, true, true, true, INT64_MAX, 61250000));
    usb.attached = true;

    /* USB stall recovery uses the same detach interval and rechecks BLE. */
    usb_source_presence_detached(&usb, 62000000);
    assert(!usb_source_presence_wanted(&usb, false, true, true, 0, 63000000));
    assert(!usb_source_presence_wanted(&usb, true, false, true, 1000000, 63000000));
    assert(usb_source_presence_wanted(&usb, true, true, true, 0, 63000000));

    /* Repeated controller cycles must not require rebooting the receiver. */
    for (int i = 0; i < 100; ++i) {
        int64_t now = 70000000LL + i * 2000000LL;
        usb.attached = true;
        assert(!usb_source_presence_wanted(&usb, false, false, false, INT64_MAX, now));
        usb_source_presence_detached(&usb, now);
        assert(usb_source_presence_wanted(&usb, true, true, true, 0, now + 1000000));
    }
    puts("USB source presence tests passed");
    return 0;
}
