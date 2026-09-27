set(KEYMAP "${CMAKE_CURRENT_LIST_DIR}/../../../config/cornix.keymap")
set(GENERATOR "${CMAKE_CURRENT_LIST_DIR}/../tools/keymap.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${KEYMAP}" "${GENERATOR}")
execute_process(COMMAND "${PYTHON}" "${GENERATOR}" "${KEYMAP}"
    "${CMAKE_CURRENT_BINARY_DIR}/keymap_generated.h" COMMAND_ERROR_IS_FATAL ANY)
idf_component_register(SRCS "main.c" "split.c" "usb.c" "engine.c" "ble_output.c" "output_route.c"
    "display.c" "display_hw.c" "display_model.c" "display_status.c" "assets/display_font.c"
    "keymap_config.c" "keymap_protocol.c" "orientation.c" "orientation_imu.c"
    "vendor/bmi270/bmi2.c" "vendor/bmi270/bmi270.c"
    INCLUDE_DIRS "." "${CMAKE_CURRENT_BINARY_DIR}"
    REQUIRES bt nvs_flash esp_timer esp_driver_gpio esp_driver_i2c esp_driver_spi esp_lcd
    EMBED_FILES "assets/cat_work.bin" "assets/cat_sleep.bin")

configure_file("vendor/bmi270/LICENSE" "${CMAKE_BINARY_DIR}/BMI270-LICENSE.txt" COPYONLY)
