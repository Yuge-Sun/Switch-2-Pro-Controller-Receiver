#pragma once

#include <stdbool.h>
#include "esp_err.h"

void device_config_init(void);
bool device_config_get_ble_autoconnect(void);
esp_err_t device_config_save_ble_autoconnect(bool enabled);
const char *device_config_get_ble_target(void);
esp_err_t device_config_save_ble_target(const char *target);
