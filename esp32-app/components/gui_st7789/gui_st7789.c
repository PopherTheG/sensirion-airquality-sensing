#include "gui_st7789.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "lvgl.h"
#include "lvgl_helpers.h"

#ifdef CONFIG_VOC_INSTALLED
#include "voc_index.h"
#endif

#ifdef CONFIG_PM_INSTALLED
#include "particulate_matter.h"
#endif

#ifdef CONFIG_CO2_INSTALLED
#include "co2.h"
#endif

#define LV_TICK_PERIOD_MS 1

#define TAG "gui_st7789.c"

/* Custom LV colors definition */
#define LV_COLOR_GOOD LV_COLOR_MAKE(0x28, 0x96, 0x3C)
#define LV_COLOR_MODERATE LV_COLOR_MAKE(0xF8, 0xFB, 0x89)
#define LV_COLOR_UNHEALTHY_FOR_SENSITIVE_GROUPS LV_COLOR_MAKE(0xF6, 0xC1, 0x42)
#define LV_COLOR_UNHEALTHY LV_COLOR_MAKE(0xDC, 0x84, 0x42)
#define LV_COLOR_VERY_UNHEALTHY LV_COLOR_MAKE(0xED, 0x64, 0x74)
#define LV_COLOR_HAZARDOUS LV_COLOR_MAKE(0xEB, 0x32, 0x23)

/* Static function prototypes for LVGL main functions */
static void lv_tick_task(void *arg);
static void guiTask(void *pvParameter);
static void create_graphics_application(void);

/* static variables for GUI components to modify it such as its styles and positioning */
static lv_style_t screen_style;
static lv_color_t hum_border_color;
static lv_color_t temp_border_color;
static lv_color_t formaldehyde_border_color;
static lv_color_t co2_border_color;
static lv_color_t pm2_5_border_color;
static lv_color_t pm10_border_color;
static lv_color_t voc_indicator_border_color;
static int8_t row2_y_offset = 110 + 5;
static int8_t row1_y_offset = 40 + 5;

static lv_style_t formaldehyde_box_style;
static lv_style_t formaldehyde_value_style;
static lv_style_t formaldehyde_bar_style;
static lv_style_t co2_box_style;
static lv_style_t co2_value_style;
static lv_style_t co2_bar_style;
static lv_style_t temp_box_style;
static lv_style_t temp_value_style;
static lv_style_t hum_box_style;
static lv_style_t hum_value_style;
static lv_style_t hum_bar_style;
static lv_style_t pm2_5_box_style;
static lv_style_t pm2_5_value_style;
static lv_style_t pm2_5_bar_style;
static lv_style_t pm10_box_style;
static lv_style_t pm10_value_style;
static lv_style_t pm10_bar_style;

/* function prototypes for callbacks to update GUI components */
static void voc_indicator_pointer_refresher_task(lv_task_t *task_info);
static void temp_label_value_refresher_task(lv_task_t *task_info);
static void hum_label_value_refresher_task(lv_task_t *task_info);
static void hum_bar_value_refresher_task(lv_task_t *task_info);
#ifdef CONFIG_HCHO_INSTALLED
static void formaldehyde_label_value_refresher_task(lv_task_t *task_info);
static void formaldehyde_bar_value_refresher_task(lv_task_t *task_info);
#endif
#ifdef CONFIG_CO2_INSTALLED
static void co2_label_value_refresher_task(lv_task_t *task_info);
static void co2_bar_value_refresher_task(lv_task_t *task_info);
#endif
#ifdef CONFIG_PM_INSTALLED
static void pm2_5_label_value_refresher_task(lv_task_t *task_info);
static void pm2_5_bar_value_refresher_task(lv_task_t *task_info);
static void pm10_label_value_refresher_task(lv_task_t *task_info);
static void pm10_bar_value_refresher_task(lv_task_t *task_info);
#endif

/**
 * Create a semphore to handle concurrent call to lvgl stuff
 * If you wish to call any lvgl function from other threads/tasks 
 * you should lock on the very same semaphore.
 */
SemaphoreHandle_t xGuiSemaphore;

static void guiTask(void *pvParameter)
{
    (void)pvParameter;
    xGuiSemaphore = xSemaphoreCreateMutex();

    lv_init();

    /* Initialize SPI or I2C bus used by the drivers */
    lvgl_driver_init();

    lv_color_t *buf1 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1 != NULL);

/* Use double buffered when not working with monochrome displays */
#ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME
    lv_color_t *buf2 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2 != NULL);
#else
    static lv_color_t *buf2 = NULL;
#endif

    static lv_disp_buf_t disp_buf;

    uint32_t size_in_px = DISP_BUF_SIZE;

#if defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_IL3820 || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_JD79653A || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_UC8151D || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_SSD1306
    /* Actual size in pixels not bytes. */
    size_in_px *= 8;
#endif

    /**
     * Initialize the working buffer depending on the selected display.
     * NOTE: buf2 == NULL when using monochrome displays
     */
    lv_disp_buf_init(&disp_buf, buf1, buf2, size_in_px);

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = disp_driver_flush;

    /**
     * When using a monochrome display we need to register the callbacks:
     * - rounder_cb
     * - set_px_cb
     */
#ifdef CONFIG_LV_TFT_DISPLAY_MONOCHROME
    disp_drv.rounder_cb = disp_driver_rounder;
    disp_drv.set_px_cb = disp_driver_set_px;
#endif

    disp_drv.buffer = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    /* Register an input device when enabled on the menuconfig */
#if CONFIG_LV_TOUCH_CONTROLLER != TOUCH_CONTROLLER_NONE
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.read_cb = touch_driver_read;
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    lv_indev_drv_register(&indev_drv);
#endif

    /* Create an start a periodic timer interrupt to call lv_tick_inc */
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .name = "periodic_gui"};
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000));

    /* Create the demo application */
    create_graphics_application();

    while (1)
    {
        /* Delay 1 tick (assumes FreeRTOS tick is 10ms) */
        vTaskDelay(pdMS_TO_TICKS(10));

        /* Try to take the semaphore, call lvgl related functions on success */
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY))
        {
            lv_task_handler();
            xSemaphoreGive(xGuiSemaphore);
        }
    }

    vTaskDelete(NULL);
}

static void create_graphics_application(void)
{
    ESP_LOGI(TAG, "Hi THERE");
    /* Screen creation */
    lv_obj_t *screen = lv_obj_create(NULL, NULL);
    lv_scr_load(screen);
    lv_style_init(&screen_style);
    lv_style_set_bg_color(&screen_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_obj_add_style(screen, LV_OBJ_PART_MAIN, &screen_style);

    hum_border_color = LV_COLOR_BLACK;
    temp_border_color = LV_COLOR_BLACK;
    formaldehyde_border_color = LV_COLOR_BLACK;
    co2_border_color = LV_COLOR_BLACK;
    pm2_5_border_color = LV_COLOR_BLACK;
    pm10_border_color = LV_COLOR_BLACK;
    voc_indicator_border_color = LV_COLOR_BLACK;

    /**
     * Creation of layout for the screen where specific data will be placed. Data such as VOC,
     * humidity, etc.
     */
    /* voc indicator related */
    lv_obj_t *voc_indicator_box = lv_obj_create(lv_scr_act(), NULL);
    lv_obj_set_size(voc_indicator_box, 240, 230 / 6);
    lv_obj_align(voc_indicator_box, lv_scr_act(), LV_ALIGN_IN_TOP_MID, 0, 0);
    static lv_style_t voc_indicator_box_style;
    lv_style_init(&voc_indicator_box_style);
    lv_style_set_bg_color(&voc_indicator_box_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_style_set_border_color(&voc_indicator_box_style, LV_STATE_DEFAULT, voc_indicator_border_color);
    lv_obj_add_style(voc_indicator_box, LV_OBJ_PART_MAIN, &voc_indicator_box_style);

    lv_obj_t *voc_indicator_title = lv_label_create(voc_indicator_box, NULL);
    lv_obj_align(voc_indicator_title, voc_indicator_box, LV_ALIGN_IN_LEFT_MID, 2, -6);
    lv_label_set_text(voc_indicator_title, "VOC");
    static lv_style_t voc_indicator_title_style;
    lv_style_init(&voc_indicator_title_style);
    lv_style_set_text_color(&voc_indicator_title_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_style_set_text_font(&voc_indicator_title_style, LV_STATE_DEFAULT, &lv_font_montserrat_22);
    lv_obj_add_style(voc_indicator_title, LV_OBJ_PART_MAIN, &voc_indicator_title_style);

    lv_obj_t *voc_indicator_pointer = lv_label_create(voc_indicator_box, NULL);
    lv_label_set_text(voc_indicator_pointer, LV_SYMBOL_EJECT);
    /* Size per color block is 25 pixels, size between color blocks is 4 pixels */
    lv_obj_set_pos(voc_indicator_pointer, 54, 20);

    static lv_style_t voc_indicator_pointer_style;
    lv_style_set_text_color(&voc_indicator_pointer_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_style_set_text_font(&voc_indicator_pointer_style, LV_STATE_DEFAULT, &lv_font_montserrat_14);
    lv_obj_add_style(voc_indicator_pointer, LV_OBJ_PART_MAIN, &voc_indicator_pointer_style);

    int8_t voc_indicator_color_box_y_offset = 5;
    int8_t voc_indicator_color_box_x_offset = 5;

    lv_obj_t *voc_indicator_green_box = lv_obj_create(voc_indicator_box, NULL);
    lv_obj_set_size(voc_indicator_green_box, 230 / 8, (230 / 12) - 2);
    lv_obj_align(voc_indicator_green_box, voc_indicator_box, LV_ALIGN_IN_TOP_LEFT, (235 / 8) * 2, voc_indicator_color_box_y_offset);
    // lv_obj_set_width_margin(voc_indicator_green_box, 0);
    static lv_style_t voc_indicator_green_box_style;
    lv_style_init(&voc_indicator_green_box_style);
    lv_style_set_radius(&voc_indicator_green_box_style, LV_STATE_DEFAULT, 0);
    lv_style_set_bg_color(&voc_indicator_green_box_style, LV_STATE_DEFAULT, LV_COLOR_GOOD);
    lv_style_set_border_color(&voc_indicator_green_box_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_obj_add_style(voc_indicator_green_box, LV_OBJ_PART_MAIN, &voc_indicator_green_box_style);

    lv_obj_t *voc_indicator_yellow_box = lv_obj_create(voc_indicator_box, NULL);
    lv_obj_set_size(voc_indicator_yellow_box, 230 / 8, (230 / 12) - 2);
    lv_obj_align(voc_indicator_yellow_box, voc_indicator_box, LV_ALIGN_IN_TOP_LEFT, (235 / 8) * 3, voc_indicator_color_box_y_offset);
    // lv_obj_set_width_margin(voc_indicator_green_box, 0);
    static lv_style_t voc_indicator_yellow_box_style;
    lv_style_init(&voc_indicator_yellow_box_style);
    lv_style_set_radius(&voc_indicator_yellow_box_style, LV_STATE_DEFAULT, 0);
    lv_style_set_bg_color(&voc_indicator_yellow_box_style, LV_STATE_DEFAULT, LV_COLOR_MODERATE);
    lv_style_set_border_color(&voc_indicator_yellow_box_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_obj_add_style(voc_indicator_yellow_box, LV_OBJ_PART_MAIN, &voc_indicator_yellow_box_style);

    lv_obj_t *voc_indicator_orange_box = lv_obj_create(voc_indicator_box, NULL);
    lv_obj_set_size(voc_indicator_orange_box, 230 / 8, (230 / 12) - 2);
    lv_obj_align(voc_indicator_orange_box, voc_indicator_box, LV_ALIGN_IN_TOP_LEFT, (235 / 8) * 4, voc_indicator_color_box_y_offset);
    static lv_style_t voc_indicator_orange_box_style;
    lv_style_init(&voc_indicator_orange_box_style);
    lv_style_set_radius(&voc_indicator_orange_box_style, LV_STATE_DEFAULT, 0);
    lv_style_set_bg_color(&voc_indicator_orange_box_style, LV_STATE_DEFAULT, LV_COLOR_UNHEALTHY_FOR_SENSITIVE_GROUPS);
    lv_style_set_border_color(&voc_indicator_orange_box_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_obj_add_style(voc_indicator_orange_box, LV_OBJ_PART_MAIN, &voc_indicator_orange_box_style);

    lv_obj_t *voc_indicator_maroon_box = lv_obj_create(voc_indicator_box, NULL);
    lv_obj_set_size(voc_indicator_maroon_box, 230 / 8, (230 / 12) - 2);
    lv_obj_align(voc_indicator_maroon_box, voc_indicator_box, LV_ALIGN_IN_TOP_LEFT, (235 / 8) * 5, voc_indicator_color_box_y_offset);
    static lv_style_t voc_indicator_maroon_box_style;
    lv_style_init(&voc_indicator_maroon_box_style);
    lv_style_set_radius(&voc_indicator_maroon_box_style, LV_STATE_DEFAULT, 0);
    lv_style_set_bg_color(&voc_indicator_maroon_box_style, LV_STATE_DEFAULT, LV_COLOR_UNHEALTHY);
    lv_style_set_border_color(&voc_indicator_maroon_box_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_obj_add_style(voc_indicator_maroon_box, LV_OBJ_PART_MAIN, &voc_indicator_maroon_box_style);

    lv_obj_t *voc_indicator_pink_box = lv_obj_create(voc_indicator_box, NULL);
    lv_obj_set_size(voc_indicator_pink_box, 230 / 8, (230 / 12) - 2);
    lv_obj_align(voc_indicator_pink_box, voc_indicator_box, LV_ALIGN_IN_TOP_LEFT, (235 / 8) * 6, voc_indicator_color_box_y_offset);
    static lv_style_t voc_indicator_pink_box_style;
    lv_style_init(&voc_indicator_pink_box_style);
    lv_style_set_radius(&voc_indicator_pink_box_style, LV_STATE_DEFAULT, 0);
    lv_style_set_bg_color(&voc_indicator_pink_box_style, LV_STATE_DEFAULT, LV_COLOR_VERY_UNHEALTHY);
    lv_style_set_border_color(&voc_indicator_pink_box_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_obj_add_style(voc_indicator_pink_box, LV_OBJ_PART_MAIN, &voc_indicator_pink_box_style);

    lv_obj_t *voc_indicator_red_box = lv_obj_create(voc_indicator_box, NULL);
    lv_obj_set_size(voc_indicator_red_box, 230 / 8, (230 / 12) - 2);
    lv_obj_align(voc_indicator_red_box, voc_indicator_box, LV_ALIGN_IN_TOP_LEFT, (235 / 8) * 7, voc_indicator_color_box_y_offset);
    static lv_style_t voc_indicator_red_box_style;
    lv_style_init(&voc_indicator_red_box_style);
    lv_style_set_radius(&voc_indicator_red_box_style, LV_STATE_DEFAULT, 0);
    lv_style_set_bg_color(&voc_indicator_red_box_style, LV_STATE_DEFAULT, LV_COLOR_HAZARDOUS);
    lv_style_set_border_color(&voc_indicator_red_box_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_obj_add_style(voc_indicator_red_box, LV_OBJ_PART_MAIN, &voc_indicator_red_box_style);

    /* Temperature related */
    lv_obj_t *temp_box = lv_obj_create(lv_scr_act(), NULL);
    lv_obj_set_size(temp_box, 230 / 2, 230 / 4);
    lv_obj_align(temp_box, lv_scr_act(), LV_ALIGN_IN_TOP_LEFT, 0, row1_y_offset);
    lv_style_init(&temp_box_style);
    lv_style_set_bg_color(&temp_box_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_style_set_border_color(&temp_box_style, LV_STATE_DEFAULT, temp_border_color);
    lv_obj_add_style(temp_box, LV_OBJ_PART_MAIN, &temp_box_style);

    lv_obj_t *temp_value = lv_label_create(temp_box, NULL);
    lv_label_set_text(temp_value, "0C");
    lv_obj_align(temp_value, temp_box, LV_ALIGN_IN_TOP_LEFT, 3, 0);
    lv_style_init(&temp_value_style);
    lv_style_set_text_font(&temp_value_style, LV_STATE_DEFAULT, &lv_font_montserrat_26);
    lv_style_set_text_color(&temp_value_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_add_style(temp_value, LV_OBJ_PART_MAIN, &temp_value_style);

    lv_obj_t *temp_label = lv_label_create(temp_box, NULL);
    lv_label_set_text(temp_label, "Temperature");
    lv_obj_align(temp_label, temp_box, LV_ALIGN_IN_BOTTOM_LEFT, 3, -2);
    static lv_style_t temp_label_style;
    lv_style_init(&temp_label_style);
    lv_style_set_text_font(&temp_label_style, LV_STATE_DEFAULT, &lv_font_montserrat_16);
    lv_style_set_text_color(&temp_label_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_add_style(temp_label, LV_OBJ_PART_MAIN, &temp_label_style);

    /* Humidity related */
    lv_obj_t *hum_box = lv_obj_create(lv_scr_act(), NULL);
    lv_obj_set_size(hum_box, 230 / 2, 230 / 4);
    lv_obj_align(hum_box, lv_scr_act(), LV_ALIGN_IN_TOP_LEFT, (230 / 2) + 10, row1_y_offset);
    lv_style_init(&hum_box_style);
    lv_style_set_bg_color(&hum_box_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_style_set_border_color(&hum_box_style, LV_STATE_DEFAULT, hum_border_color);
    lv_obj_add_style(hum_box, LV_OBJ_PART_MAIN, &hum_box_style);

    lv_obj_t *hum_value = lv_label_create(hum_box, NULL);
    lv_label_set_text(hum_value, "0%");
    lv_obj_align(hum_value, hum_box, LV_ALIGN_IN_TOP_LEFT, 12, 0);
    lv_style_init(&hum_value_style);
    lv_style_set_text_font(&hum_value_style, LV_STATE_DEFAULT, &lv_font_montserrat_26);
    lv_style_set_text_color(&hum_value_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_add_style(hum_value, LV_OBJ_PART_MAIN, &hum_value_style);

    lv_obj_t *hum_bar = lv_bar_create(hum_box, NULL);
    lv_obj_set_size(hum_bar, (8), (230 / 4) - 10);
    lv_obj_align(hum_bar, hum_box, LV_ALIGN_IN_LEFT_MID, 0, 0);
    lv_bar_set_value(hum_bar, 100, LV_ANIM_OFF);
    lv_style_init(&hum_bar_style);
    lv_style_set_bg_color(&hum_bar_style, LV_STATE_DEFAULT, LV_COLOR_GRAY);
    lv_obj_add_style(hum_bar, LV_BAR_PART_INDIC, &hum_bar_style);

    lv_obj_t *hum_label = lv_label_create(hum_box, NULL);
    lv_label_set_text(hum_label, "Humidity");
    lv_obj_align(hum_label, hum_box, LV_ALIGN_IN_BOTTOM_LEFT, 12, -2);
    static lv_style_t hum_label_style;
    lv_style_init(&hum_label_style);
    lv_style_set_text_font(&hum_label_style, LV_STATE_DEFAULT, &lv_font_montserrat_16);
    lv_style_set_text_color(&hum_label_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_add_style(hum_label, LV_OBJ_PART_MAIN, &hum_label_style);

    /* Formaldehyde related */
#ifdef CONFIG_HCHO_INSTALLED
    lv_obj_t *formaldehyde_box = lv_obj_create(lv_scr_act(), NULL);
    lv_obj_set_size(formaldehyde_box, 230 / 2, 230 / 4);
    lv_obj_align(formaldehyde_box, lv_scr_act(), LV_ALIGN_IN_TOP_LEFT, 0, row2_y_offset);
    lv_style_init(&formaldehyde_box_style);
    lv_style_set_bg_color(&formaldehyde_box_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_style_set_border_color(&formaldehyde_box_style, LV_STATE_DEFAULT, formaldehyde_border_color);
    lv_obj_add_style(formaldehyde_box, LV_OBJ_PART_MAIN, &formaldehyde_box_style);

    lv_obj_t *formaldehyde_value = lv_label_create(formaldehyde_box, NULL);
    lv_label_set_text(formaldehyde_value, "0");
    lv_obj_align(formaldehyde_value, formaldehyde_box, LV_ALIGN_IN_TOP_LEFT, 10, 0);
    lv_style_init(&formaldehyde_value_style);
    lv_style_set_text_font(&formaldehyde_value_style, LV_STATE_DEFAULT, &lv_font_montserrat_28);
    lv_style_set_text_color(&formaldehyde_value_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_add_style(formaldehyde_value, LV_OBJ_PART_MAIN, &formaldehyde_value_style);

    lv_obj_t *formaldehyde_bar = lv_bar_create(formaldehyde_box, NULL);
    lv_obj_set_size(formaldehyde_bar, (8), (230 / 4) - 10);
    lv_obj_align(formaldehyde_bar, formaldehyde_box, LV_ALIGN_IN_LEFT_MID, 0, 0);
    lv_bar_set_value(formaldehyde_bar, 100, LV_ANIM_OFF);
    lv_style_init(&formaldehyde_bar_style);
    lv_style_set_bg_color(&formaldehyde_bar_style, LV_STATE_DEFAULT, LV_COLOR_GRAY);
    lv_obj_add_style(formaldehyde_bar, LV_BAR_PART_INDIC, &formaldehyde_bar_style);

    lv_obj_t *formaldehyde_label = lv_label_create(formaldehyde_box, NULL);
    lv_label_set_text(formaldehyde_label, "HCHO");
    lv_obj_align(formaldehyde_label, formaldehyde_box, LV_ALIGN_IN_BOTTOM_LEFT, 10, -2);
    static lv_style_t formaldehyde_label_style;
    lv_style_init(&formaldehyde_label_style);
    lv_style_set_text_font(&formaldehyde_label_style, LV_STATE_DEFAULT, &lv_font_montserrat_18);
    lv_style_set_text_color(&formaldehyde_label_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_add_style(formaldehyde_label, LV_OBJ_PART_MAIN, &formaldehyde_label_style);
#else
    lv_obj_t *formaldehyde_box = lv_obj_create(lv_scr_act(), NULL);
    lv_obj_set_size(formaldehyde_box, 230 / 2, 230 / 4);
    lv_obj_align(formaldehyde_box, lv_scr_act(), LV_ALIGN_IN_TOP_LEFT, 0, row2_y_offset);
    lv_style_init(&formaldehyde_box_style);
    lv_style_set_bg_color(&formaldehyde_box_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_style_set_border_color(&formaldehyde_box_style, LV_STATE_DEFAULT, formaldehyde_border_color);
    lv_obj_add_style(formaldehyde_box, LV_OBJ_PART_MAIN, &formaldehyde_box_style);

    lv_obj_t *formaldehyde_value = lv_label_create(formaldehyde_box, NULL);
    lv_label_set_text(formaldehyde_value, "N/A");
    lv_obj_align(formaldehyde_value, formaldehyde_box, LV_ALIGN_IN_TOP_LEFT, 10, 0);
    lv_style_init(&formaldehyde_value_style);
    lv_style_set_text_font(&formaldehyde_value_style, LV_STATE_DEFAULT, &lv_font_montserrat_28);
    lv_style_set_text_color(&formaldehyde_value_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_add_style(formaldehyde_value, LV_OBJ_PART_MAIN, &formaldehyde_value_style);

    lv_obj_t *formaldehyde_bar = lv_bar_create(formaldehyde_box, NULL);
    lv_obj_set_size(formaldehyde_bar, (8), (230 / 4) - 10);
    lv_obj_align(formaldehyde_bar, formaldehyde_box, LV_ALIGN_IN_LEFT_MID, 0, 0);
    lv_bar_set_value(formaldehyde_bar, 100, LV_ANIM_OFF);
    lv_style_init(&formaldehyde_bar_style);
    lv_style_set_bg_color(&formaldehyde_bar_style, LV_STATE_DEFAULT, LV_COLOR_GRAY);
    lv_obj_add_style(formaldehyde_bar, LV_BAR_PART_INDIC, &formaldehyde_bar_style);

    lv_obj_t *formaldehyde_label = lv_label_create(formaldehyde_box, NULL);
    lv_label_set_text(formaldehyde_label, "HCHO");
    lv_obj_align(formaldehyde_label, formaldehyde_box, LV_ALIGN_IN_BOTTOM_LEFT, 10, -2);
    static lv_style_t formaldehyde_label_style;
    lv_style_init(&formaldehyde_label_style);
    lv_style_set_text_font(&formaldehyde_label_style, LV_STATE_DEFAULT, &lv_font_montserrat_18);
    lv_style_set_text_color(&formaldehyde_label_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_add_style(formaldehyde_label, LV_OBJ_PART_MAIN, &formaldehyde_label_style);
#endif

    /* CO2 related */
#ifdef CONFIG_CO2_INSTALLED
    lv_obj_t *co2_box = lv_obj_create(lv_scr_act(), NULL);
    lv_obj_set_size(co2_box, 230 / 2, 230 / 4);
    lv_obj_align(co2_box, lv_scr_act(), LV_ALIGN_IN_TOP_LEFT, (230 / 2) + 10, row2_y_offset);
    lv_style_init(&co2_box_style);
    lv_style_set_bg_color(&co2_box_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_style_set_border_color(&co2_box_style, LV_STATE_DEFAULT, co2_border_color);
    lv_obj_add_style(co2_box, LV_OBJ_PART_MAIN, &co2_box_style);

    lv_obj_t *co2_value = lv_label_create(co2_box, NULL);
    lv_label_set_text(co2_value, "0");
    lv_obj_align(co2_value, co2_box, LV_ALIGN_IN_TOP_LEFT, 10, 0);
    lv_style_init(&co2_value_style);
    lv_style_set_text_font(&co2_value_style, LV_STATE_DEFAULT, &lv_font_montserrat_28);
    lv_style_set_text_color(&co2_value_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_add_style(co2_value, LV_OBJ_PART_MAIN, &co2_value_style);

    lv_obj_t *co2_bar = lv_bar_create(co2_box, NULL);
    lv_obj_set_size(co2_bar, (8), (230 / 4) - 10);
    lv_obj_align(co2_bar, co2_box, LV_ALIGN_IN_LEFT_MID, 0, 0);
    lv_bar_set_value(co2_bar, 100, LV_ANIM_OFF);
    lv_style_init(&co2_bar_style);
    lv_style_set_bg_color(&co2_bar_style, LV_STATE_DEFAULT, LV_COLOR_GRAY);
    lv_obj_add_style(co2_bar, LV_BAR_PART_INDIC, &co2_bar_style);

    lv_obj_t *co2_label = lv_label_create(co2_box, NULL);
    lv_label_set_text(co2_label, "CO2 (ppm)");
    lv_obj_align(co2_label, co2_box, LV_ALIGN_IN_BOTTOM_LEFT, 10, -2);
    static lv_style_t co2_label_style;
    lv_style_init(&co2_label_style);
    lv_style_set_text_font(&co2_label_style, LV_STATE_DEFAULT, &lv_font_montserrat_16);
    lv_style_set_text_color(&co2_label_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_add_style(co2_label, LV_OBJ_PART_MAIN, &co2_label_style);
#else
    lv_obj_t *co2_box = lv_obj_create(lv_scr_act(), NULL);
    lv_obj_set_size(co2_box, 230 / 2, 230 / 4);
    lv_obj_align(co2_box, lv_scr_act(), LV_ALIGN_IN_TOP_LEFT, (230 / 2) + 10, row2_y_offset);
    lv_style_init(&co2_box_style);
    lv_style_set_bg_color(&co2_box_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_style_set_border_color(&co2_box_style, LV_STATE_DEFAULT, co2_border_color);
    lv_obj_add_style(co2_box, LV_OBJ_PART_MAIN, &co2_box_style);

    lv_obj_t *co2_value = lv_label_create(co2_box, NULL);
    lv_label_set_text(co2_value, "N/A");
    lv_obj_align(co2_value, co2_box, LV_ALIGN_IN_TOP_LEFT, 10, 0);
    lv_style_init(&co2_value_style);
    lv_style_set_text_font(&co2_value_style, LV_STATE_DEFAULT, &lv_font_montserrat_28);
    lv_style_set_text_color(&co2_value_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_add_style(co2_value, LV_OBJ_PART_MAIN, &co2_value_style);

    lv_obj_t *co2_bar = lv_bar_create(co2_box, NULL);
    lv_obj_set_size(co2_bar, (8), (230 / 4) - 10);
    lv_obj_align(co2_bar, co2_box, LV_ALIGN_IN_LEFT_MID, 0, 0);
    lv_bar_set_value(co2_bar, 100, LV_ANIM_OFF);
    lv_style_init(&co2_bar_style);
    lv_style_set_bg_color(&co2_bar_style, LV_STATE_DEFAULT, LV_COLOR_GRAY);
    lv_obj_add_style(co2_bar, LV_BAR_PART_INDIC, &co2_bar_style);

    lv_obj_t *co2_label = lv_label_create(co2_box, NULL);
    lv_label_set_text(co2_label, "CO2 (ppm)");
    lv_obj_align(co2_label, co2_box, LV_ALIGN_IN_BOTTOM_LEFT, 10, -2);
    static lv_style_t co2_label_style;
    lv_style_init(&co2_label_style);
    lv_style_set_text_font(&co2_label_style, LV_STATE_DEFAULT, &lv_font_montserrat_16);
    lv_style_set_text_color(&co2_label_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_add_style(co2_label, LV_OBJ_PART_MAIN, &co2_label_style);
#endif

    /* Particulate matter sensor */
#ifdef CONFIG_PM_INSTALLED
    /* PM2.5 Related */
    lv_obj_t *pm2_5_box = lv_obj_create(lv_scr_act(), NULL);
    lv_obj_set_size(pm2_5_box, 230 / 2, 230 / 4);
    lv_obj_align(pm2_5_box, lv_scr_act(), LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);
    lv_style_init(&pm2_5_box_style);
    lv_style_set_bg_color(&pm2_5_box_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_style_set_border_color(&pm2_5_box_style, LV_STATE_DEFAULT, pm2_5_border_color);
    lv_obj_add_style(pm2_5_box, LV_OBJ_PART_MAIN, &pm2_5_box_style);

    lv_obj_t *pm2_5_value = lv_label_create(pm2_5_box, NULL);
    lv_label_set_text(pm2_5_value, "0");
    lv_obj_align(pm2_5_value, pm2_5_box, LV_ALIGN_IN_TOP_LEFT, 10, 0);
    lv_style_init(&pm2_5_value_style);
    lv_style_set_text_font(&pm2_5_value_style, LV_STATE_DEFAULT, &lv_font_montserrat_28);
    lv_style_set_text_color(&pm2_5_value_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_add_style(pm2_5_value, LV_OBJ_PART_MAIN, &pm2_5_value_style);

    lv_obj_t *pm2_5_bar = lv_bar_create(pm2_5_box, NULL);
    lv_obj_set_size(pm2_5_bar, (8), (230 / 4) - 10);
    lv_obj_align(pm2_5_bar, pm2_5_box, LV_ALIGN_IN_LEFT_MID, 0, 0);
    lv_bar_set_value(pm2_5_bar, 100, LV_ANIM_OFF);
    lv_style_init(&pm2_5_bar_style);
    lv_style_set_bg_color(&pm2_5_bar_style, LV_STATE_DEFAULT, LV_COLOR_GRAY);
    lv_obj_add_style(pm2_5_bar, LV_BAR_PART_INDIC, &pm2_5_bar_style);

    lv_obj_t *pm2_5_label = lv_label_create(pm2_5_box, NULL);
    lv_label_set_text(pm2_5_label, "PM2.5 (ug/m3)");
    lv_obj_align(pm2_5_label, pm2_5_box, LV_ALIGN_IN_BOTTOM_LEFT, 10, 0);
    static lv_style_t pm2_5_label_style;
    lv_style_init(&pm2_5_label_style);
    lv_style_set_text_font(&pm2_5_label_style, LV_STATE_DEFAULT, &lv_font_montserrat_12);
    lv_style_set_text_color(&pm2_5_label_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_add_style(pm2_5_label, LV_OBJ_PART_MAIN, &pm2_5_label_style);

    /* PM10 Related */
    lv_obj_t *pm10_box = lv_obj_create(lv_scr_act(), NULL);
    lv_obj_set_size(pm10_box, 230 / 2, 230 / 4);
    lv_obj_align(pm10_box, lv_scr_act(), LV_ALIGN_IN_BOTTOM_LEFT, (230 / 2) + 10, 0);
    lv_style_init(&pm10_box_style);
    lv_style_set_bg_color(&pm10_box_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_style_set_border_color(&pm10_box_style, LV_STATE_DEFAULT, pm10_border_color);
    lv_obj_add_style(pm10_box, LV_OBJ_PART_MAIN, &pm10_box_style);

    lv_obj_t *pm10_value = lv_label_create(pm10_box, NULL);
    lv_label_set_text(pm10_value, "0");
    lv_obj_align(pm10_value, pm10_box, LV_ALIGN_IN_TOP_LEFT, 10, 0);
    lv_style_init(&pm10_value_style);
    lv_style_set_text_font(&pm10_value_style, LV_STATE_DEFAULT, &lv_font_montserrat_28);
    lv_style_set_text_color(&pm10_value_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_add_style(pm10_value, LV_OBJ_PART_MAIN, &pm10_value_style);

    lv_obj_t *pm10_bar = lv_bar_create(pm10_box, NULL);
    lv_obj_set_size(pm10_bar, (8), (230 / 4) - 10);
    lv_obj_align(pm10_bar, pm10_box, LV_ALIGN_IN_LEFT_MID, 0, 0);
    lv_bar_set_value(pm10_bar, 100, LV_ANIM_OFF);
    lv_style_init(&pm10_bar_style);
    lv_style_set_bg_color(&pm10_bar_style, LV_STATE_DEFAULT, LV_COLOR_GRAY);
    lv_obj_add_style(pm10_bar, LV_BAR_PART_INDIC, &pm10_bar_style);

    lv_obj_t *pm10_label = lv_label_create(pm10_box, NULL);
    lv_label_set_text(pm10_label, "PM10 (ug/m3)");
    lv_obj_align(pm10_label, pm10_box, LV_ALIGN_IN_BOTTOM_LEFT, 10, 0);
    static lv_style_t pm10_label_style;
    lv_style_init(&pm10_label_style);
    lv_style_set_text_font(&pm10_label_style, LV_STATE_DEFAULT, &lv_font_montserrat_12);
    lv_style_set_text_color(&pm10_label_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_add_style(pm10_label, LV_OBJ_PART_MAIN, &pm10_label_style);
#else
    /* PM2.5 Related */
    lv_obj_t *pm2_5_box = lv_obj_create(lv_scr_act(), NULL);
    lv_obj_set_size(pm2_5_box, 230 / 2, 230 / 4);
    lv_obj_align(pm2_5_box, lv_scr_act(), LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);
    lv_style_init(&pm2_5_box_style);
    lv_style_set_bg_color(&pm2_5_box_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_style_set_border_color(&pm2_5_box_style, LV_STATE_DEFAULT, pm2_5_border_color);
    lv_obj_add_style(pm2_5_box, LV_OBJ_PART_MAIN, &pm2_5_box_style);

    lv_obj_t *pm2_5_value = lv_label_create(pm2_5_box, NULL);
    lv_label_set_text(pm2_5_value, "N/A");
    lv_obj_align(pm2_5_value, pm2_5_box, LV_ALIGN_IN_TOP_LEFT, 10, 0);
    lv_style_init(&pm2_5_value_style);
    lv_style_set_text_font(&pm2_5_value_style, LV_STATE_DEFAULT, &lv_font_montserrat_28);
    lv_style_set_text_color(&pm2_5_value_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_add_style(pm2_5_value, LV_OBJ_PART_MAIN, &pm2_5_value_style);

    lv_obj_t *pm2_5_bar = lv_bar_create(pm2_5_box, NULL);
    lv_obj_set_size(pm2_5_bar, (8), (230 / 4) - 10);
    lv_obj_align(pm2_5_bar, pm2_5_box, LV_ALIGN_IN_LEFT_MID, 0, 0);
    lv_bar_set_value(pm2_5_bar, 100, LV_ANIM_OFF);
    lv_style_init(&pm2_5_bar_style);
    lv_style_set_bg_color(&pm2_5_bar_style, LV_STATE_DEFAULT, LV_COLOR_GRAY);
    lv_obj_add_style(pm2_5_bar, LV_BAR_PART_INDIC, &pm2_5_bar_style);

    lv_obj_t *pm2_5_label = lv_label_create(pm2_5_box, NULL);
    lv_label_set_text(pm2_5_label, "PM2.5 (ug/m3)");
    lv_obj_align(pm2_5_label, pm2_5_box, LV_ALIGN_IN_BOTTOM_LEFT, 10, 0);
    static lv_style_t pm2_5_label_style;
    lv_style_init(&pm2_5_label_style);
    lv_style_set_text_font(&pm2_5_label_style, LV_STATE_DEFAULT, &lv_font_montserrat_12);
    lv_style_set_text_color(&pm2_5_label_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_add_style(pm2_5_label, LV_OBJ_PART_MAIN, &pm2_5_label_style);

    /* PM10 Related */
    lv_obj_t *pm10_box = lv_obj_create(lv_scr_act(), NULL);
    lv_obj_set_size(pm10_box, 230 / 2, 230 / 4);
    lv_obj_align(pm10_box, lv_scr_act(), LV_ALIGN_IN_BOTTOM_LEFT, (230 / 2) + 10, 0);
    lv_style_init(&pm10_box_style);
    lv_style_set_bg_color(&pm10_box_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_style_set_border_color(&pm10_box_style, LV_STATE_DEFAULT, pm10_border_color);
    lv_obj_add_style(pm10_box, LV_OBJ_PART_MAIN, &pm10_box_style);

    lv_obj_t *pm10_value = lv_label_create(pm10_box, NULL);
    lv_label_set_text(pm10_value, "N/A");
    lv_obj_align(pm10_value, pm10_box, LV_ALIGN_IN_TOP_LEFT, 10, 0);
    lv_style_init(&pm10_value_style);
    lv_style_set_text_font(&pm10_value_style, LV_STATE_DEFAULT, &lv_font_montserrat_28);
    lv_style_set_text_color(&pm10_value_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_add_style(pm10_value, LV_OBJ_PART_MAIN, &pm10_value_style);

    lv_obj_t *pm10_bar = lv_bar_create(pm10_box, NULL);
    lv_obj_set_size(pm10_bar, (8), (230 / 4) - 10);
    lv_obj_align(pm10_bar, pm10_box, LV_ALIGN_IN_LEFT_MID, 0, 0);
    lv_bar_set_value(pm10_bar, 100, LV_ANIM_OFF);
    lv_style_init(&pm10_bar_style);
    lv_style_set_bg_color(&pm10_bar_style, LV_STATE_DEFAULT, LV_COLOR_GRAY);
    lv_obj_add_style(pm10_bar, LV_BAR_PART_INDIC, &pm10_bar_style);

    lv_obj_t *pm10_label = lv_label_create(pm10_box, NULL);
    lv_label_set_text(pm10_label, "PM10 (ug/m3)");
    lv_obj_align(pm10_label, pm10_box, LV_ALIGN_IN_BOTTOM_LEFT, 10, 0);
    static lv_style_t pm10_label_style;
    lv_style_init(&pm10_label_style);
    lv_style_set_text_font(&pm10_label_style, LV_STATE_DEFAULT, &lv_font_montserrat_12);
    lv_style_set_text_color(&pm10_label_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_add_style(pm10_label, LV_OBJ_PART_MAIN, &pm10_label_style);
#endif

    /**
     * lv tasks creation for dynamic value handling
     */
    /* VOC related */
#ifdef CONFIG_VOC_INSTALLED
    lv_task_create(voc_indicator_pointer_refresher_task, 250, LV_TASK_PRIO_MID, (void *)voc_indicator_pointer);
    /* Temperature related */
    lv_task_create(temp_label_value_refresher_task, 250, LV_TASK_PRIO_MID, (void *)temp_value);
    /* Humidity related */
    lv_task_create(hum_label_value_refresher_task, 250, LV_TASK_PRIO_MID, (void *)hum_value);
    lv_task_create(hum_bar_value_refresher_task, 250, LV_TASK_PRIO_MID, (void *)hum_bar);
#endif
    /* Formaldehyde related */
#ifdef CONFIG_HCHO_INSTALLED
    lv_task_create(formaldehyde_label_value_refresher_task, 250, LV_TASK_PRIO_MID, (void *)formaldehyde_value);
    lv_task_create(formaldehyde_bar_value_refresher_task, 250, LV_TASK_PRIO_MID, (void *)formaldehyde_bar);
#endif
    /* CO2 related */
#ifdef CONFIG_CO2_INSTALLED
    lv_task_create(co2_label_value_refresher_task, 250, LV_TASK_PRIO_MID, (void *)co2_value);
    lv_task_create(co2_bar_value_refresher_task, 250, LV_TASK_PRIO_MID, (void *)co2_bar);
#endif
    /* Particulate matter related*/
#ifdef CONFIG_PM_INSTALLED
    lv_task_create(pm2_5_label_value_refresher_task, 250, LV_TASK_PRIO_MID, (void *)pm2_5_value);
    lv_task_create(pm2_5_bar_value_refresher_task, 250, LV_TASK_PRIO_MID, (void *)pm2_5_bar);
    lv_task_create(pm10_label_value_refresher_task, 250, LV_TASK_PRIO_MID, (void *)pm10_value);
    lv_task_create(pm10_bar_value_refresher_task, 250, LV_TASK_PRIO_MID, (void *)pm10_bar);
#endif
}

#ifdef CONFIG_HCHO_INSTALLED
static void formaldehyde_label_value_refresher_task(lv_task_t *task_info)
{
}

static void formaldehyde_bar_value_refresher_task(lv_task_t *task_info)
{
}
#endif

#ifdef CONFIG_VOC_INSTALLED
static void voc_indicator_pointer_refresher_task(lv_task_t *task_info)
{
    int16_t voc_index;
    voc_index_get_voc(&voc_index);
    float real_voc = voc_index / 10.0;

    /* This conditional statements is for adjusting the position of the pointer depending on the VOC value.
    Each color block has its how scale since not all of them has the same values in between */
    if (real_voc >= 0 && real_voc <= 50)
    {
        uint8_t offset = real_voc / (51 / 25);
        lv_obj_set_pos((lv_obj_t *)(task_info->user_data), 54 + offset, 20);
    }
    else if (real_voc >= 51 && real_voc <= 100)
    {
        uint8_t offset = (real_voc - 50) / (50 / 25);
        lv_obj_set_pos((lv_obj_t *)(task_info->user_data), 83 + offset, 20); /* Start position of MODERATE */
    }
    else if (real_voc >= 101 && real_voc <= 150)
    {
        uint8_t offset = (real_voc - 100) / (50 / 25);
        lv_obj_set_pos((lv_obj_t *)(task_info->user_data), 112 + offset, 20); /* Start position of UNHEALTHY FOR SENSITIVE GROUPS */
    }
    else if (real_voc >= 151 && real_voc <= 200)
    {
        uint8_t offset = (real_voc - 150) / (50 / 25);
        lv_obj_set_pos((lv_obj_t *)(task_info->user_data), 141 + offset, 20); /* Start position of UNHEALTHY */
    }
    else if (real_voc >= 201 && real_voc <= 300)
    {
        uint8_t offset = (real_voc - 200) / (100 / 25);
        lv_obj_set_pos((lv_obj_t *)(task_info->user_data), 170 + offset, 20); /* Start position of VERY UNHEALTHY */
    }
    else if (real_voc >= 301 && real_voc <= 500)
    {
        uint8_t offset = (real_voc - 300) / (200 / 25);
        lv_obj_set_pos((lv_obj_t *)(task_info->user_data), 199 + offset, 20); /* Start position of HAZARDOUS */
    }
}
#endif

#ifdef CONFIG_VOC_INSTALLED
static void temp_label_value_refresher_task(lv_task_t *task_info)
{
    int16_t temperature;
    voc_index_get_temperature(&temperature);
    float real_temperature = temperature / 200.0;
    lv_label_set_text_fmt((lv_obj_t *)(task_info->user_data), "%.01fC", real_temperature);
}
#endif

#ifdef CONFIG_VOC_INSTALLED
static void hum_label_value_refresher_task(lv_task_t *task_info)
{
    int16_t humidity;
    voc_index_get_rhumidity(&humidity);
    float real_humidity = humidity / 100.0;
    lv_label_set_text_fmt((lv_obj_t *)(task_info->user_data), "%0.1f%%", real_humidity);
}
#endif

#ifdef CONFIG_VOC_INSTALLED
static void hum_bar_value_refresher_task(lv_task_t *task_info)
{
    int16_t humidity;
    voc_index_get_rhumidity(&humidity);
    float real_humidity = humidity / 100.0;
    lv_bar_set_value((lv_obj_t *)(task_info->user_data), 100, LV_ANIM_OFF); // set it at 100 just to fill in the bar's color.
    // Source for color assignment: https://www.airthings.com/en/what-is-humidity
    if (real_humidity < 25)
    {
        lv_style_set_bg_color(&hum_bar_style, LV_STATE_DEFAULT, LV_COLOR_HAZARDOUS);
        lv_obj_add_style((lv_obj_t *)(task_info->user_data), LV_BAR_PART_INDIC, &hum_bar_style);
    }
    else if (real_humidity >= 25 && real_humidity < 30)
    {
        lv_style_set_bg_color(&hum_bar_style, LV_STATE_DEFAULT, LV_COLOR_UNHEALTHY);
        lv_obj_add_style((lv_obj_t *)(task_info->user_data), LV_BAR_PART_INDIC, &hum_bar_style);
    }
    else if (real_humidity >= 30 && real_humidity < 60)
    {
        lv_style_set_bg_color(&hum_bar_style, LV_STATE_DEFAULT, LV_COLOR_GOOD);
        lv_obj_add_style((lv_obj_t *)(task_info->user_data), LV_BAR_PART_INDIC, &hum_bar_style);
    }
    else if (real_humidity >= 60 && real_humidity < 70)
    {
        lv_style_set_bg_color(&hum_bar_style, LV_STATE_DEFAULT, LV_COLOR_UNHEALTHY);
        lv_obj_add_style((lv_obj_t *)(task_info->user_data), LV_BAR_PART_INDIC, &hum_bar_style);
    }
    else if (real_humidity >= 70)
    {
        lv_style_set_bg_color(&hum_bar_style, LV_STATE_DEFAULT, LV_COLOR_HAZARDOUS);
        lv_obj_add_style((lv_obj_t *)(task_info->user_data), LV_BAR_PART_INDIC, &hum_bar_style);
    }
}
#endif

#ifdef CONFIG_CO2_INSTALLED
static void co2_label_value_refresher_task(lv_task_t *task_info)
{
    uint16_t co2;
    co2_get_co2(&co2);
    lv_label_set_text_fmt((lv_obj_t *)(task_info->user_data), "%i", co2);
}
static void co2_bar_value_refresher_task(lv_task_t *task_info)
{
    uint16_t co2;
    co2_get_co2(&co2);

    lv_bar_set_value((lv_obj_t *)(task_info->user_data), 100, LV_ANIM_OFF); // set it at 100 just to fill in the bar's color.

    // https://greenecon.net/3-metrics-to-guide-air-quality-health-safety/carbon-footprint.html
    if (co2 <= 700)
    {
        lv_style_set_bg_color(&co2_bar_style, LV_STATE_DEFAULT, LV_COLOR_GOOD);
        lv_obj_add_style((lv_obj_t *)(task_info->user_data), LV_BAR_PART_INDIC, &co2_bar_style);
    }
    else if (co2 > 700 && co2 <= 1000)
    {
        lv_style_set_bg_color(&co2_bar_style, LV_STATE_DEFAULT, LV_COLOR_MODERATE);
        lv_obj_add_style((lv_obj_t *)(task_info->user_data), LV_BAR_PART_INDIC, &co2_bar_style);
    }
    else if (co2 > 1000 && co2 <= 1500)
    {
        lv_style_set_bg_color(&co2_bar_style, LV_STATE_DEFAULT, LV_COLOR_UNHEALTHY_FOR_SENSITIVE_GROUPS);
        lv_obj_add_style((lv_obj_t *)(task_info->user_data), LV_BAR_PART_INDIC, &co2_bar_style);
    }
    else if (co2 > 1500 && co2 <= 2500)
    {
        lv_style_set_bg_color(&co2_bar_style, LV_STATE_DEFAULT, LV_COLOR_UNHEALTHY);
        lv_obj_add_style((lv_obj_t *)(task_info->user_data), LV_BAR_PART_INDIC, &co2_bar_style);
    }
    else if (co2 > 2500 && co2 <= 5000)
    {
        lv_style_set_bg_color(&co2_bar_style, LV_STATE_DEFAULT, LV_COLOR_VERY_UNHEALTHY);
        lv_obj_add_style((lv_obj_t *)(task_info->user_data), LV_BAR_PART_INDIC, &co2_bar_style);
    }
    else if (co2 > 5000)
    {
        lv_style_set_bg_color(&co2_bar_style, LV_STATE_DEFAULT, LV_COLOR_HAZARDOUS);
        lv_obj_add_style((lv_obj_t *)(task_info->user_data), LV_BAR_PART_INDIC, &co2_bar_style);
    }
}
#endif

#ifdef CONFIG_PM_INSTALLED
static void pm2_5_label_value_refresher_task(lv_task_t *task_info)
{
    uint16_t pm2p5;
    particulate_matter_get_pm2p5(&pm2p5);
    float real_pm2p5 = pm2p5 / 1000.0;
    lv_label_set_text_fmt((lv_obj_t *)(task_info->user_data), "%.01f", real_pm2p5);
}
static void pm2_5_bar_value_refresher_task(lv_task_t *task_info)
{
    uint16_t pm2p5;
    particulate_matter_get_pm2p5(&pm2p5);
    float real_pm2p5 = pm2p5 / 1000.0;

    lv_bar_set_value((lv_obj_t *)(task_info->user_data), 100, LV_ANIM_OFF); // set it at 100 just to fill in the bar's color.

    // https://www.airveda.com/blog/Understanding-Particulate-Matter-and-Its-Associated-Health-Impact
    if (real_pm2p5 <= 30)
    {
        lv_style_set_bg_color(&pm2_5_bar_style, LV_STATE_DEFAULT, LV_COLOR_GOOD);
        lv_obj_add_style((lv_obj_t *)(task_info->user_data), LV_BAR_PART_INDIC, &pm2_5_bar_style);
    }
    else if (real_pm2p5 > 30 && real_pm2p5 <= 60)
    {
        lv_style_set_bg_color(&pm2_5_bar_style, LV_STATE_DEFAULT, LV_COLOR_MODERATE);
        lv_obj_add_style((lv_obj_t *)(task_info->user_data), LV_BAR_PART_INDIC, &pm2_5_bar_style);
    }
    else if (real_pm2p5 > 60 && real_pm2p5 <= 90)
    {
        lv_style_set_bg_color(&pm2_5_bar_style, LV_STATE_DEFAULT, LV_COLOR_UNHEALTHY_FOR_SENSITIVE_GROUPS);
        lv_obj_add_style((lv_obj_t *)(task_info->user_data), LV_BAR_PART_INDIC, &pm2_5_bar_style);
    }
    else if (real_pm2p5 > 90 && real_pm2p5 <= 120)
    {
        lv_style_set_bg_color(&pm2_5_bar_style, LV_STATE_DEFAULT, LV_COLOR_UNHEALTHY);
        lv_obj_add_style((lv_obj_t *)(task_info->user_data), LV_BAR_PART_INDIC, &pm2_5_bar_style);   
    }
    else if (real_pm2p5 > 120 && real_pm2p5 <= 250)
    {
        lv_style_set_bg_color(&pm2_5_bar_style, LV_STATE_DEFAULT, LV_COLOR_VERY_UNHEALTHY);
        lv_obj_add_style((lv_obj_t *)(task_info->user_data), LV_BAR_PART_INDIC, &pm2_5_bar_style);
    }
    else if (real_pm2p5 > 250)
    {
        lv_style_set_bg_color(&pm2_5_bar_style, LV_STATE_DEFAULT, LV_COLOR_HAZARDOUS);
        lv_obj_add_style((lv_obj_t *)(task_info->user_data), LV_BAR_PART_INDIC, &pm2_5_bar_style);
    }
}
static void pm10_label_value_refresher_task(lv_task_t *task_info)
{
    uint16_t pm10p0;
    particulate_matter_get_pm10p0(&pm10p0);
    float real_pm10p0 = pm10p0 / 1000.0;
    lv_label_set_text_fmt((lv_obj_t *)(task_info->user_data), "%.01f", real_pm10p0);
}
static void pm10_bar_value_refresher_task(lv_task_t *task_info)
{
    uint16_t pm10p0;
    particulate_matter_get_pm10p0(&pm10p0);
    float real_pm10p0 = pm10p0 / 1000.0;

    lv_bar_set_value((lv_obj_t *)(task_info->user_data), 100, LV_ANIM_OFF); // set it at 100 just to fill in the bar's color.

    // https://www.airveda.com/blog/Understanding-Particulate-Matter-and-Its-Associated-Health-Impact
    if (real_pm10p0 <= 50)
    {
        lv_style_set_bg_color(&pm10_bar_style, LV_STATE_DEFAULT, LV_COLOR_GOOD);
        lv_obj_add_style((lv_obj_t *)(task_info->user_data), LV_BAR_PART_INDIC, &pm10_bar_style);
    }
    else if (real_pm10p0 > 50 && real_pm10p0 <= 100)
    {
        lv_style_set_bg_color(&pm10_bar_style, LV_STATE_DEFAULT, LV_COLOR_MODERATE);
        lv_obj_add_style((lv_obj_t *)(task_info->user_data), LV_BAR_PART_INDIC, &pm10_bar_style);
    }
    else if (real_pm10p0 > 100 && real_pm10p0 <= 250)
    {
        lv_style_set_bg_color(&pm10_bar_style, LV_STATE_DEFAULT, LV_COLOR_UNHEALTHY_FOR_SENSITIVE_GROUPS);
        lv_obj_add_style((lv_obj_t *)(task_info->user_data), LV_BAR_PART_INDIC, &pm10_bar_style);
    }
    else if (real_pm10p0 > 250 && real_pm10p0 <= 350)
    {
        lv_style_set_bg_color(&pm10_bar_style, LV_STATE_DEFAULT, LV_COLOR_UNHEALTHY);
        lv_obj_add_style((lv_obj_t *)(task_info->user_data), LV_BAR_PART_INDIC, &pm10_bar_style);   
    }
    else if (real_pm10p0 > 350 && real_pm10p0 <= 430)
    {
        lv_style_set_bg_color(&pm10_bar_style, LV_STATE_DEFAULT, LV_COLOR_VERY_UNHEALTHY);
        lv_obj_add_style((lv_obj_t *)(task_info->user_data), LV_BAR_PART_INDIC, &pm10_bar_style);
    }
    else if (real_pm10p0 > 430)
    {
        lv_style_set_bg_color(&pm10_bar_style, LV_STATE_DEFAULT, LV_COLOR_HAZARDOUS);
        lv_obj_add_style((lv_obj_t *)(task_info->user_data), LV_BAR_PART_INDIC, &pm10_bar_style);
    }
}
#endif

static void lv_tick_task(void *arg)
{
    (void)arg;
    lv_tick_inc(LV_TICK_PERIOD_MS);
}

void gui_st7789_init()
{
    /**
     * If you want to use a task to create the graphic, yoyu NEED to create a pinned task.
     * Otherwise, there can be problem such as memory corruption and so on.
     * NOTE: When not using Wi-Fi nor Bluetooth you can pin the task to core 0.
     */
    xTaskCreatePinnedToCore(guiTask, "st7789 gui", 4096 * 2, NULL, 0, NULL, 1);
}