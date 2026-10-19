#include "device_config.h"
#include <stdio.h>
#include "nvs.h"
#include "app_log.h"

static const char *TAG = "config";
// Keep the existing namespace and keys to preserve saved pairing settings.
static const char *NVS_NAMESPACE = "bridge";
static const char *NVS_KEY_BLE_AUTO = "ble_auto";
static const char *NVS_KEY_BLE_TARGET = "ble_target";
#define BLE_TARGET_MAX_LEN 40

static bool s_ble_autoconnect = true;
static char s_ble_target[BLE_TARGET_MAX_LEN];

void device_config_init(void)
{
    s_ble_autoconnect = true;
    s_ble_target[0] = 0;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        uint8_t stored_auto = 1;
        err = nvs_get_u8(handle, NVS_KEY_BLE_AUTO, &stored_auto);
        if (err == ESP_OK) {
            s_ble_autoconnect = stored_auto != 0;
        } else if (err == ESP_ERR_NVS_NOT_FOUND) {
            APP_LOGI(TAG, "no persisted BLE autoconnect; defaulting to enabled");
        } else {
            APP_LOGW(TAG, "failed to read BLE autoconnect err=%d; defaulting to enabled", (int)err);
        }

        size_t target_len = sizeof(s_ble_target);
        err = nvs_get_str(handle, NVS_KEY_BLE_TARGET, s_ble_target, &target_len);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            s_ble_target[0] = 0;
            APP_LOGI(TAG, "no persisted BLE target; autoconnect will scan");
        } else if (err != ESP_OK) {
            s_ble_target[0] = 0;
            APP_LOGW(TAG, "failed to read BLE target err=%d; autoconnect will scan", (int)err);
        } else {
            s_ble_target[sizeof(s_ble_target) - 1] = 0;
        }
        nvs_close(handle);
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        APP_LOGW(TAG, "failed to read BLE config err=%d", (int)err);
    }
    APP_LOGI(TAG, "PS5 receiver ble_auto=%s ble_target=%s",
             s_ble_autoconnect ? "on" : "off",
             s_ble_target[0] ? s_ble_target : "<scan>");
}

bool device_config_get_ble_autoconnect(void)
{
    return s_ble_autoconnect;
}

esp_err_t device_config_save_ble_autoconnect(bool enabled)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        APP_LOGW(TAG, "failed to open config namespace for BLE auto write err=%d", (int)err);
        return err;
    }

    err = nvs_set_u8(handle, NVS_KEY_BLE_AUTO, enabled ? 1 : 0);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err == ESP_OK) {
        s_ble_autoconnect = enabled;
        APP_LOGI(TAG, "BLE autoconnect saved: %s", enabled ? "on" : "off");
    } else {
        APP_LOGW(TAG, "failed to save BLE autoconnect err=%d", (int)err);
    }
    return err;
}

const char *device_config_get_ble_target(void)
{
    return s_ble_target;
}

esp_err_t device_config_save_ble_target(const char *target)
{
    char sanitized[BLE_TARGET_MAX_LEN];
    snprintf(sanitized, sizeof(sanitized), "%s", target ? target : "");

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        APP_LOGW(TAG, "failed to open config namespace for BLE target write err=%d", (int)err);
        return err;
    }

    if (sanitized[0]) {
        err = nvs_set_str(handle, NVS_KEY_BLE_TARGET, sanitized);
    } else {
        err = nvs_erase_key(handle, NVS_KEY_BLE_TARGET);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            err = ESP_OK;
        }
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err == ESP_OK) {
        snprintf(s_ble_target, sizeof(s_ble_target), "%s", sanitized);
        APP_LOGI(TAG, "BLE target saved: %s", s_ble_target[0] ? s_ble_target : "<scan>");
    } else {
        APP_LOGW(TAG, "failed to save BLE target err=%d", (int)err);
    }
    return err;
}

