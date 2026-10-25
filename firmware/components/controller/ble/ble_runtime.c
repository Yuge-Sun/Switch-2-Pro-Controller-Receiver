#include "ble_runtime.h"

#include <stdbool.h>
#include "esp_err.h"
#include "esp_pm.h"
#include "sdkconfig.h"
#include "esp_bt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#define BLE_CONNECTED_BIT (1U << 0)

static SemaphoreHandle_t s_lock;
static EventGroupHandle_t s_events;
static esp_pm_lock_handle_t s_cpu_lock;
static ble_state_t s_state = BLE_STATE_IDLE;
static bool s_cpu_locked;
#if CONFIG_BT_CTRL_MODEM_SLEEP
static bool s_controller_ready;
static bool s_modem_sleep_enabled;

static void update_modem_sleep(bool active)
{
    if (s_controller_ready && s_modem_sleep_enabled == active) {
        ESP_ERROR_CHECK(active ? esp_bt_sleep_disable() : esp_bt_sleep_enable());
        s_modem_sleep_enabled = !active;
    }
}
#endif

void ble_runtime_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    s_events = xEventGroupCreate();
    ESP_ERROR_CHECK(s_lock && s_events ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0,
                                       "ble_link", &s_cpu_lock));
}

void ble_runtime_controller_ready(void)
{
#if CONFIG_BT_CTRL_MODEM_SLEEP
    xSemaphoreTake(s_lock, portMAX_DELAY);
    /* ESP-IDF enables configured modem sleep when it enables the controller. */
    s_controller_ready = true;
    s_modem_sleep_enabled = true;
    update_modem_sleep(s_state == BLE_STATE_CONNECTING || s_state == BLE_STATE_CONNECTED);
    xSemaphoreGive(s_lock);
#endif
}

ble_state_t ble_runtime_state(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    ble_state_t state = s_state;
    xSemaphoreGive(s_lock);
    return state;
}

void ble_runtime_set_state(ble_state_t state)
{
    /* GAP callbacks and management tasks can both change state. Serialize the
     * state, persistent event and recursive PM lock so they cannot diverge.
     */
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool active = state == BLE_STATE_CONNECTING || state == BLE_STATE_CONNECTED;
    if (active && !s_cpu_locked) {
        ESP_ERROR_CHECK(esp_pm_lock_acquire(s_cpu_lock));
        s_cpu_locked = true;
    }
#if CONFIG_BT_CTRL_MODEM_SLEEP
    /* Preserve the connected timing behavior: only disconnected scanning uses
     * modem sleep. CPU light sleep remains disabled independently. */
    update_modem_sleep(active);
#endif
    s_state = state;
    if (state == BLE_STATE_CONNECTED) {
        xEventGroupSetBits(s_events, BLE_CONNECTED_BIT);
    } else {
        xEventGroupClearBits(s_events, BLE_CONNECTED_BIT);
    }
    if (!active && s_cpu_locked) {
        ESP_ERROR_CHECK(esp_pm_lock_release(s_cpu_lock));
        s_cpu_locked = false;
    }
    xSemaphoreGive(s_lock);
}

void ble_runtime_wait_connected(void)
{
    /* Do not clear on exit: both input and rumble must observe the same event,
     * including workers that have not reached this wait yet.
     */
    xEventGroupWaitBits(s_events, BLE_CONNECTED_BIT, pdFALSE, pdTRUE,
                       portMAX_DELAY);
}
