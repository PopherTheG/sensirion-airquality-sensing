idf_component_register(
    SRCS "particulate_matter.c"
    INCLUDE_DIRS "."
    REQUIRES "freertos" "log" "sps30" "sensirion_common"
)