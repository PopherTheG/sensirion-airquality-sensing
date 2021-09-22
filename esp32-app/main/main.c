#include <stdio.h>
#include <stdbool.h>
#include <string.h>

/* esp specific includes */
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"

/* gui component includes */
#include "gui_st7789.h"
/* Only include if it is checked in sdkconfig */
#ifdef CONFIG_VOC_INSTALLED
/* voc index includes */
#include "voc_index.h"
#endif

/* Only include if it is checked in sdkconfig */
#ifdef CONFIG_PM_INSTALLED
/* particulate matter includes */
#include "particulate_matter.h"
#endif

/* Only include if it is checked in sdkconfig */
#ifdef CONFIG_CO2_INSTALLED
/* co2 includes */
#include "co2.h"
#endif

#include "telemetry.h"

#define TAG "main.c"

/* GPIO pin used for I2C bus one */
#define I2C1_SDA 27
#define I2C1_SCL 14

/* Static functions prototype */
static void system_init();

void app_main(void)
{
    int16_t voc_index, rhumidity, temperature;
    uint16_t co2;
    uint16_t pm2p5, pm10p0;

    system_init();

#ifdef CONFIG_VOC_INSTALLED
    /* Start voc index component. This shoud be called first before you can retrieve values
    from the sensor */
    voc_index_init();
    vTaskDelay(pdMS_TO_TICKS(1000));
#endif

#ifdef CONFIG_PM_INSTALLED
    /* Start particulate matter component. This should be called first before you can retrieve 
    values from the sensor. */
    particulate_matter_init();
    vTaskDelay(pdMS_TO_TICKS(1000));
#endif

#ifdef CONFIG_CO2_INSTALLED
    /* Start co2 component. This should be called first before you can retrieve values from the
    sensor. */
    co2_init();
    vTaskDelay(pdMS_TO_TICKS(1000));
#endif

    /* Start displaying on the ST7789 TFT display */
    gui_st7789_init();

    /* Initialize telemetry. Initialize this after setting up all the sensors and cloud connection */
    telemetry_init();

    while (1)
    {
#ifdef CONFIG_VOC_INSTALLED
        voc_index_get_voc(&voc_index);
        float real_voc = voc_index / 10.0;
        voc_index_get_temperature(&temperature);
        float real_temperature = temperature / 200.0;
        voc_index_get_rhumidity(&rhumidity);
        float real_rhumidity = rhumidity / 100.0;

        /* Log sensor values from VOC sensor */
        ESP_LOGI(TAG, "VOC: %.02f   Temperature: %.02f  Relative humidity: %.02f", real_voc, real_temperature, real_rhumidity);
#endif

#ifdef CONFIG_CO2_INSTALLED
        co2_get_co2(&co2);
        /* Log sensor values from CO2 sensor*/
        ESP_LOGI(TAG, "CO2: %i", co2);
#endif

#ifdef CONFIG_PM_INSTALLED
        particulate_matter_get_pm10p0(&pm10p0);
        float real_pm10p0 = pm10p0 / 1000.0;
        particulate_matter_get_pm2p5(&pm2p5);
        float real_pm2p5 = pm2p5 / 1000.0;
        ESP_LOGI(TAG, "PM2.5: %0.2f PM10.0: %.02f", real_pm2p5, real_pm10p0);
#endif
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

static void system_init()
{
    /* Initialize I2C port 1 */ /* This is where Sensirion sensors are connected */
    i2c_config_t i2c1_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C1_SDA,
        .scl_io_num = I2C1_SCL,
        .sda_pullup_en = true,
        .scl_pullup_en = true,
        .master.clk_speed = 100000};
    i2c_param_config(I2C_NUM_1, &i2c1_config);
    i2c_driver_install(I2C_NUM_1, i2c1_config.mode, 0, 0, 0);

    /* Initialise WiFi */

}
