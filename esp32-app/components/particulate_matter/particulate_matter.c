#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "particulate_matter.h"

#include "sps30.h"
#include "sensirion_i2c_hal.h"

#define TAG "particulate_matter.c"

static void particulate_matter_task(void *pvParameters);

static float s_pm10p0, s_pm2p5;

static void particulate_matter_task(void *pvParameters)
{
    struct sps30_measurement m;
    int16_t ret;

    /* NOTE: it is assumed that the I2C bus that this sensor is connected to is
    initialized in 'main.c' */

    while (sps30_probe() != 0)
    {
        ESP_LOGE(TAG, "SPS sensor probing failed\n");
        sensirion_i2c_hal_sleep_usec(1000000); /* wait 1s */
    }
    ESP_LOGI(TAG, "SPS sensor probing successful\n");

    uint8_t fw_major;
    uint8_t fw_minor;
    ret = sps30_read_firmware_version(&fw_major, &fw_minor);
    if (ret)
    {
        ESP_LOGE(TAG, "error reading firmware version\n");
    }
    else
    {
        ESP_LOGI(TAG, "FW: %u.%u\n", fw_major, fw_minor);
    }

    char serial_number[SPS30_MAX_SERIAL_LEN];
    ret = sps30_get_serial(serial_number);
    if (ret)
    {
        ESP_LOGE(TAG, "error reading serial number\n");
    }
    else
    {
        ESP_LOGI(TAG, "Serial Number: %s\n", serial_number);
    }

    ret = sps30_start_measurement();
    if (ret < 0)
        ESP_LOGE(TAG, "error starting measurement\n");
    ESP_LOGI(TAG, "measurements started\n");

    while (1)
    {
        sensirion_i2c_hal_sleep_usec(SPS30_MEASUREMENT_DURATION_USEC); /* wait 1s */
        ret = sps30_read_measurement(&m);
        if (ret < 0)
        {
            ESP_LOGE(TAG, "error reading measurement.");
        }
        else
        {
            s_pm2p5 = m.mc_2p5;
            s_pm10p0 = m.mc_10p0;
        }
    }
}

void particulate_matter_init()
{
    xTaskCreate(particulate_matter_task, "PM task", 1024 * 4, NULL, 5, NULL);
}

void particulate_matter_get_pm10p0(uint16_t *pm10p0)
{
    *pm10p0 = (uint16_t)(s_pm10p0);
}

void particulate_matter_get_pm2p5(uint16_t *pm2p5)
{
    *pm2p5 = (uint16_t)(s_pm2p5);
}