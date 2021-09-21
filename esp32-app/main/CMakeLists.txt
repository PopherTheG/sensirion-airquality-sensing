idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES "gui_st7789" "voc_index" "particulate_matter" "freertos" "driver" "log" "co2" "telemetry"
)