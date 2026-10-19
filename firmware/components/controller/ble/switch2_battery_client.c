#include "switch2_battery_client.h"

#include <string.h>
#include <stdatomic.h>
#include "app_log.h"
#include "esp_timer.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "nimble/nimble_port.h"
#include "os/os_mbuf.h"
#include "switch2_state.h"

// A brief primary Notify must never become a second gameplay input stream.
// Ported from the pure-software Pro2InitialBatterySnapshot / RuntimeBattery policy.
#define FIRST_REFRESH_US 30000000LL
#define REFRESH_US 120000000LL
#define RATE_WINDOW_US 10000000LL
#define OPERATION_GAP_US 45000LL

typedef enum {
    STOPPED, IDLE, CHECK_NONE, ENABLING, WAIT_NOTIFY, DISABLING,
    VERIFY_NONE, OBSERVING, SUSPENDED
} battery_phase_t;

static const char *TAG = "battery";
static struct ble_npl_callout s_timer;
static struct ble_npl_event s_input_ready_event;
static atomic_uintptr_t s_ready_epoch;
static bool s_timer_ready;
static battery_phase_t s_phase;
static uint16_t s_conn, s_cccd;
static atomic_uintptr_t s_epoch;
static atomic_bool s_active;
static void (*s_initial_ready)(void);
static bool s_initial, s_dirty, s_captured;
static pro2_battery_t s_pending;
static int64_t s_deadline, s_due, s_last_fd2, s_window_start;
static uint32_t s_count, s_max_gap, s_before_count, s_operation_gap;
static bool s_monitor_operation;

static int battery_gatt_cb(uint16_t conn, const struct ble_gatt_error *error,
                           struct ble_gatt_attr *attr, void *arg);
static void battery_timer_cb(struct ble_npl_event *event);
static void battery_input_ready_cb(struct ble_npl_event *event);
static void cleanup_subscription(void);

static void initial_ready(void)
{
    if (!s_active) return;
    void (*ready)(void) = s_initial_ready;
    s_initial_ready = NULL;
    if (ready) ready();
}

static void suspend_refresh(const char *reason)
{
    s_phase = SUSPENDED;
    s_monitor_operation = false;
    APP_LOGW(TAG, "[PRO2_BATTERY] suspended reason=%s", reason);
    initial_ready(); // Capture failure alone must not prevent gameplay.
}

static void unsafe_subscription(const char *reason)
{
    // Stop late callbacks, and reconnect rather than leave primary Notify on.
    ++s_epoch;
    s_phase = SUSPENDED;
    s_initial_ready = NULL;
    s_monitor_operation = false;
    APP_LOGW(TAG, "[PRO2_BATTERY] cleanup_unverified reason=%s reconnect=true", reason);
    int rc = ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
    if (rc != 0) APP_LOGW(TAG, "[PRO2_BATTERY] terminate rc=%d", rc);
}

static void arm_timer(uint32_t ms)
{
    if (ble_npl_callout_reset(&s_timer, ble_npl_time_ms_to_ticks32(ms)) != 0) {
        if (s_dirty) unsafe_subscription("timer_failed");
        else suspend_refresh("timer_failed");
    }
}

static void reset_window(int64_t now)
{
    s_window_start = now;
    s_count = 0;
    s_max_gap = 0;
}

static void retry_refresh(const char *reason)
{
    // Only retry when we have not enabled Notify, or have verified cleanup.
    if (s_dirty) { unsafe_subscription(reason); return; }
    ++s_epoch; // Ignore callbacks from the failed attempt.
    s_phase = IDLE;
    s_monitor_operation = false;
    s_captured = false;
    int64_t now = esp_timer_get_time();
    s_due = now + REFRESH_US;
    reset_window(now);
    APP_LOGW(TAG, "[PRO2_BATTERY] retry_pending reason=%s cooldown_s=120", reason);
    arm_timer(100);
    initial_ready();
}

static bool rate_stable(int64_t now)
{
    int64_t duration = now - s_window_start;
    // Conservative bounded-window substitute for the Windows percentile capture:
    // require >=128 real parsed FD2 samples/s and no gap above 45 ms.
    return duration >= RATE_WINDOW_US && duration <= RATE_WINDOW_US + 1000000LL &&
           (uint64_t)s_count * 1000000ULL >= (uint64_t)duration * 128ULL &&
           s_last_fd2 > 0 && now - s_last_fd2 < OPERATION_GAP_US &&
           s_max_gap <= OPERATION_GAP_US;
}

static void start_read(battery_phase_t phase)
{
    if (!s_active) return;
    s_phase = phase;
    s_deadline = esp_timer_get_time() + 2000000LL;
    ++s_epoch;
    int rc = ble_gattc_read(s_conn, s_cccd, battery_gatt_cb, (void *)s_epoch);
    if (rc != 0) {
        if (s_dirty) unsafe_subscription("cccd_read_start_failed");
        else retry_refresh("cccd_read_start_failed");
        return;
    }
    arm_timer(20);
}

static void start_write(battery_phase_t phase, uint16_t value)
{
    if (!s_active) return;
    uint8_t data[2] = {(uint8_t)value, 0};
    s_phase = phase;
    s_deadline = esp_timer_get_time() + 2000000LL;
    ++s_epoch;
    // Mark dirty BEFORE starting enable; an ATT timeout can still leave Notify on.
    s_dirty = true;
    int rc = ble_gattc_write_flat(s_conn, s_cccd, data, sizeof(data),
                                  battery_gatt_cb, (void *)s_epoch);
    if (rc != 0) {
        if (phase == ENABLING) cleanup_subscription();
        else unsafe_subscription("cccd_disable_start_failed");
        return;
    }
    arm_timer(20);
}

static void cleanup_subscription(void)
{
    start_write(DISABLING, 0);
}

static void commit_reading(void)
{
    if (!s_active) return;
    switch2_state_store_battery(&s_pending);
    APP_LOGI(TAG, "[PRO2_BATTERY] level=%u/9 approx_percent=%u charging=%s external_power=%s source=%s",
             s_pending.level, pro2_battery_percent(&s_pending),
             s_pending.charging ? "true" : "false",
             s_pending.external_power ? "true" : "false",
             s_initial ? "connection" : "runtime");
}

static int battery_gatt_cb(uint16_t conn, const struct ble_gatt_error *error,
                           struct ble_gatt_attr *attr, void *arg)
{
    if (!s_active || conn != s_conn || (uintptr_t)arg != s_epoch ||
        s_phase == STOPPED || s_phase == SUSPENDED) return 0;
    if (error->status != 0) {
        if (s_phase == ENABLING) cleanup_subscription();
        else if (s_dirty) unsafe_subscription("cccd_operation_failed");
        else retry_refresh("cccd_read_failed");
        return 0;
    }
    if (s_phase == CHECK_NONE || s_phase == VERIFY_NONE) {
        uint8_t value[2];
        if (!attr || !attr->om || OS_MBUF_PKTLEN(attr->om) != 2 ||
            os_mbuf_copydata(attr->om, 0, 2, value) != 0 || value[0] != 0 || value[1] != 0) {
            unsafe_subscription("cccd_not_none");
            return 0;
        }
        if (s_phase == CHECK_NONE) {
            start_write(ENABLING, 1);
        } else {
            s_dirty = false;
            APP_LOGI(TAG, "[PRO2_BATTERY] primary_notify_off verified=true");
            if (!s_captured) {
                retry_refresh("no_valid_notification");
            } else if (s_initial) {
                commit_reading();
                s_phase = IDLE;
                ble_npl_callout_stop(&s_timer);
                initial_ready();
            } else {
                // Continue measuring after cleanup; don't accept a reading until
                // the FD2 stream is proven to have resumed at the original rate.
                s_phase = OBSERVING;
                s_deadline = esp_timer_get_time() + RATE_WINDOW_US;
                reset_window(esp_timer_get_time());
                arm_timer(20);
            }
        }
    } else if (s_phase == ENABLING) {
        s_phase = WAIT_NOTIFY;
        s_deadline = esp_timer_get_time() + (s_initial ? 2000000LL : 500000LL);
        if (s_captured) cleanup_subscription(); // Notify can precede write ACK.
        else arm_timer(20);
    } else if (s_phase == DISABLING) {
        start_read(VERIFY_NONE);
    }
    return 0;
}

static void battery_timer_cb(struct ble_npl_event *event)
{
    (void)event;
    if (!s_active) return;
    int64_t now = esp_timer_get_time();
    if (s_phase == IDLE) {
        if (now - s_window_start >= RATE_WINDOW_US) {
            bool stable = rate_stable(now);
            if (now >= s_due && stable) {
                s_initial = false;
                s_captured = false;
                s_before_count = s_count;
                s_operation_gap = 0;
                s_monitor_operation = true;
                s_due = now + REFRESH_US;
                start_read(CHECK_NONE);
                return;
            }
            reset_window(now);
        }
        arm_timer(100);
    } else if (s_phase == OBSERVING) {
        // Include the gap across cleanup and the following notification.
        if (now >= s_deadline) {
            bool stable = rate_stable(now) &&
                (uint64_t)s_count * 100 >= (uint64_t)s_before_count * 94 &&
                s_operation_gap <= OPERATION_GAP_US;
            s_monitor_operation = false;
            if (!stable) retry_refresh("fd2_timing_changed");
            else {
                commit_reading();
                s_phase = IDLE;
                reset_window(now);
                arm_timer(100);
            }
        } else arm_timer(20);
    } else if (s_phase >= CHECK_NONE && s_phase <= VERIFY_NONE) {
        if (now < s_deadline) arm_timer(20);
        else if (s_phase == WAIT_NOTIFY) cleanup_subscription();
        else if (s_dirty) unsafe_subscription("cccd_timeout");
        else retry_refresh("cccd_timeout");
    }
}

void switch2_battery_client_init(void)
{
    ble_npl_callout_init(&s_timer, nimble_port_get_dflt_eventq(), battery_timer_cb, NULL);
    ble_npl_event_init(&s_input_ready_event, battery_input_ready_cb, NULL);
    s_timer_ready = true;
    switch2_battery_client_reset();
}

void switch2_battery_client_reset(void)
{
    // May be called from the reconnect/control task. Invalidate atomically;
    // only the host task (start) reinitializes the rest of the state machine.
    s_active = false;
    if (s_timer_ready) ble_npl_callout_stop(&s_timer);
    ++s_epoch; // Reject old callbacks even when a connection handle is recycled.
}

void switch2_battery_client_start(uint16_t conn, uint16_t value, uint16_t cccd,
                                 void (*ready)(void))
{
    switch2_battery_client_reset();
    s_conn = conn;
    s_phase = STOPPED;
    s_dirty = s_captured = s_monitor_operation = false;
    s_last_fd2 = 0;
    s_due = 0;
    s_active = true;
    s_cccd = cccd;
    s_initial = true;
    s_initial_ready = ready;
    if (!value || !cccd) { suspend_refresh("characteristic_unavailable"); return; }
    APP_LOGI(TAG, "[PRO2_BATTERY] snapshot_before_fd2=true interval_s=120");
    start_read(CHECK_NONE);
}

void switch2_battery_client_input_ready(void)
{
    // Subscription completion runs in the existing FreeRTOS scheduler task.
    // Move window/timer mutations onto the same host queue as receive callbacks.
    if (!s_active) return;
    s_ready_epoch = s_epoch;
    ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &s_input_ready_event);
}

static void battery_input_ready_cb(struct ble_npl_event *event)
{
    (void)event;
    if (s_ready_epoch != s_epoch) return;
    if (!s_active || s_phase != IDLE) return;
    int64_t now = esp_timer_get_time();
    if (s_due < now + FIRST_REFRESH_US) s_due = now + FIRST_REFRESH_US;
    reset_window(now);
    arm_timer(100);
}

void switch2_battery_client_primary(uint16_t conn, const uint8_t *data, uint16_t len)
{
    if (!s_active || conn != s_conn || (s_phase != ENABLING && s_phase != WAIT_NOTIFY) || s_captured) return;
    if (!pro2_battery_decode_primary(data, len, esp_timer_get_time(), &s_pending)) return;
    s_captured = true;
    if (s_phase == WAIT_NOTIFY) cleanup_subscription();
}

void switch2_battery_client_fd2(uint16_t conn, const uint8_t *data, uint16_t len)
{
    if (!s_active || conn != s_conn || s_phase == STOPPED || len < 60) return;
    int64_t now = esp_timer_get_time();
    if (s_last_fd2 > 0 && now > s_last_fd2) {
        uint32_t gap = (uint32_t)(now - s_last_fd2);
        if (gap > s_max_gap) s_max_gap = gap;
        if (s_monitor_operation && gap > s_operation_gap) s_operation_gap = gap;
    }
    s_last_fd2 = now;
    ++s_count;
    switch2_state_store_battery_fd2(data, len, now);
}
