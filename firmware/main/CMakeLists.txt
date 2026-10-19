idf_component_register(
    SRCS
        "app_main.c"
        "usb_dualsense_descriptor.c"
        "usb_uac1_audio_driver.c"
        "dualsense_report.c"
        "dualsense_report_mapper.c"
        "dualsense_rumble_intent.c"
        "dualsense_haptic_audio.c"
        "haptic_audio_to_raw02.c"
        "pro2_input_backend.c"
        "pro2_rumble_arbiter.c"
        "pro2_rumble_backend.c"
        "v55_control_protocol.c"
    INCLUDE_DIRS "." "tinyusb_config"
    REQUIRES esp_tinyusb controller nvs_flash esp_timer esp_hw_support
    WHOLE_ARCHIVE
)

# Compile TinyUSB with the same configuration as the custom DualSense driver.
idf_component_get_property(tinyusb_lib espressif__tinyusb COMPONENT_LIB)
target_include_directories(${tinyusb_lib} BEFORE PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/tinyusb_config")
