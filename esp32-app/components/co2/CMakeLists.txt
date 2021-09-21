idf_component_register(
    SRCS "co2.c"
    INCLUDE_DIRS "."
    REQUIRES "freertos" "log" "scd41"
)