#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "co2.h"

#include "scd4x_i2c.h"
#include "sensirion_common.h"
#include "sensirion_i2c_hal.h"

#define TAG "co2.c"

static void co2_task(void *pvParameters);

static uint16_t s_co2;

static void co2_task(void *pvParameters)
{
    /* NOTE: it is assumed that the I2C bus that is connected to is initialized
    in 'main.c' */

    int16_t error = 0;

    // Clean up potential SCD40 states
    scd4x_wake_up();
    scd4x_stop_periodic_measurement();
    scd4x_reinit();

    uint16_t serial_0;
    uint16_t serial_1;
    uint16_t serial_2;
    error = scd4x_get_serial_number(&serial_0, &serial_1, &serial_2);
    if (error)
    {
        ESP_LOGE(TAG, "Error executing scd4x_get_serial_number(): %i\n", error);
    }
    else
    {
        ESP_LOGI(TAG, "serial: 0x%04x%04x%04x\n", serial_0, serial_1, serial_2);
    }

    // Start Measurement

    error = scd4x_start_periodic_measurement();
    if (error)
    {
        ESP_LOGE(TAG, "Error executing scd4x_start_periodic_measurement(): %i\n",
                 error);
    }

    ESP_LOGI(TAG, "Waiting for first measurement... (5 sec)\n");

    for (;;)
    {
        // Read Measurement
        sensirion_i2c_hal_sleep_usec(5000000);

        uint16_t co2;
        int32_t temperature;
        int32_t humidity;
        error = scd4x_read_measurement(&co2, &temperature, &humidity);
        if (error)
        {
            ESP_LOGE(TAG, "Error executing scd4x_read_measurement(): %i\n", error);
        }
        else if (co2 == 0)
        {
            ESP_LOGE(TAG, "Invalid sample detected, skipping.\n");
        }
        else
        {
            s_co2 = co2;
        }
    }
}

void co2_init()
{
    xTaskCreate(co2_task, "co2 task", 1024 * 4, NULL, 5, NULL);
}

void co2_get_co2(uint16_t *co2)
{
    *co2 = s_co2;
}