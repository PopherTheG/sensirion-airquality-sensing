idf_component_register(
    SRCS "gui_st7789.c"
    INCLUDE_DIRS "."
    REQUIRES freertos driver esp_system esp_common lvgl lvgl_esp32_drivers voc_index
)