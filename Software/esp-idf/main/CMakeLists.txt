idf_component_register(SRCS "rs485_example.c"
                    REQUIRES nvs_flash esp_driver_uart esp_driver_gpio
                    INCLUDE_DIRS ".")
