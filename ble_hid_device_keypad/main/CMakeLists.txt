idf_component_register(SRCS "keypad.c" "ble_hidd_demo_main.c"
                            "esp_hidd_prf_api.c"
                            "hid_dev.c"
                            "hid_device_le_prf.c"
                            "esp32_button.c"
                    INCLUDE_DIRS "." "include")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-const-variable)
