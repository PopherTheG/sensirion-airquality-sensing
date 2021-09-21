idf_component_register(
    SRCS "sensirion_common.c" "sensirion_i2c_hal.c" "sensirion_i2c.c" "sensirion_shdlc.c" "sensirion_uart_hal.c"
    INCLUDE_DIRS "."
    REQUIRES "freertos" "driver" "esp_common"
)