idf_component_register(
    SRCS "telemetry.c"
    INCLUDE_DIRS "."
    REQUIRES "co2" "particulate_matter" "voc_index" "freertos" "log"
)