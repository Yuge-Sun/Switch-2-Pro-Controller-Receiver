set(BRIDGE_MAIN "${CMAKE_CURRENT_LIST_DIR}/../../../esp32s3_switch2_bridge/main")

idf_component_register(
    SRCS
        "${BRIDGE_MAIN}/ble/ble_central.c"
        "${BRIDGE_MAIN}/ble/switch2_pairing.c"
        "${BRIDGE_MAIN}/ble/ble_dual_probe.c"
        "${BRIDGE_MAIN}/ble/switch2_gatt.c"
        "${BRIDGE_MAIN}/bridge/switch2_state.c"
        "${BRIDGE_MAIN}/bridge/internal_gamepad_state.c"
        "${BRIDGE_MAIN}/bridge/gamepad_axis_math.c"
        "${BRIDGE_MAIN}/bridge/normalized_rumble.c"
        "${BRIDGE_MAIN}/config/device_config.c"
        "${BRIDGE_MAIN}/log/app_log.c"
    INCLUDE_DIRS
        "${BRIDGE_MAIN}/ble"
        "${BRIDGE_MAIN}/bridge"
        "${BRIDGE_MAIN}/config"
        "${BRIDGE_MAIN}/log"
    REQUIRES
        bt
        nvs_flash
        esp_timer
        mbedtls
        esp_hw_support
)

# Opt-in logging build; normal firmware retains its inactivity timer.
if(PRO2_PAIRING_DIAGNOSTICS)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PRO2_PAIRING_DIAGNOSTICS=1)
endif()
