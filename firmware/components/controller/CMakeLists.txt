idf_component_register(
    SRCS
        "ble/ble_central.c"
        "ble/ble_runtime.c"
        "ble/switch2_battery_client.c"
        "ble/switch2_pairing.c"
        "ble/switch2_gatt.c"
        "bridge/switch2_state.c"
        "bridge/internal_gamepad_state.c"
        "bridge/gamepad_axis_math.c"
        "bridge/normalized_rumble.c"
        "config/device_config.c"
        "log/app_log.c"
    INCLUDE_DIRS "ble" "bridge" "config" "log"
    REQUIRES bt nvs_flash esp_timer mbedtls esp_hw_support esp_pm
)
