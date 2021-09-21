#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "voc_index.h"

#include "sensirion_common.h"
#include "sensirion_i2c_hal.h"
#include "svm40_i2c.h"

#define TAG "voc_index.c"

static void voc_index_task(void *pvParameters);

static int16_t s_voc_index, s_relative_humidity, s_temperature;

static void voc_index_task(void *pvParameters)
{
    int16_t error = 0;

    sensirion_i2c_hal_init();

    uint8_t serial_number[26];
    uint8_t serial_number_size = 26;
    error = svm40_get_serial_number(&serial_number[0], serial_number_size);
    if (error)
    {
        ESP_LOGE(TAG, "Error executing svm40_get_serial_number(): %i\n", error);
    }
    else
    {
        ESP_LOGI(TAG, "Serial number: %s\n", serial_number);
    }

    uint8_t firmware_major;
    uint8_t firmware_minor;
    bool firmware_debug;
    uint8_t hardware_major;
    uint8_t hardware_minor;
    uint8_t protocol_major;
    uint8_t protocol_minor;
    error = svm40_get_version(&firmware_major, &firmware_minor, &firmware_debug,
                              &hardware_major, &hardware_minor, &protocol_major,
                              &protocol_minor);
    if (error)
    {
        ESP_LOGE(TAG, "Error executing svm40_get_version(): %i\n", error);
    }
    else
    {
        ESP_LOGI(TAG, "Firmware: %i.%i Debug: %i\n", firmware_major, firmware_minor,
               firmware_debug);
        ESP_LOGI(TAG, "Hardware: %i.%i\n", hardware_major, hardware_minor);
        ESP_LOGI(TAG, "Protocol: %i.%i\n", protocol_major, protocol_minor);
    }

    if (firmware_major < 2)
    {
        ESP_LOGE(TAG, "Your SVM40 firmware is out of date!\nPlease visit "
               "https://www.sensirion.com/my-sgp-ek/ for more information.\n");
    }

    int16_t t_offset;
    error = svm40_get_temperature_offset_for_rht_measurements(&t_offset);
    if (error)
    {
        ESP_LOGE(TAG, "Error executing "
               "svm40_get_temperature_offset_for_rht_measurements(): %i\n",
               error);
    }
    else
    {
        ESP_LOGI(TAG, "Temperature Offset: %i ticks\n", t_offset);
    }

    // Start Measurement
    error = svm40_start_continuous_measurement();
    if (error)
    {
        ESP_LOGE(TAG, "Error executing svm40_start_continuous_measurement(): %i\n",
               error);
    }

    while (1)
    {
        // Read Measurement
        sensirion_i2c_hal_sleep_usec(1000000);
        int16_t voc_index;
        int16_t humidity;
        int16_t temperature;
        error = svm40_read_measured_values_as_integers(&voc_index, &humidity,
                                                       &temperature);
        if (error)
        {
            ESP_LOGE(TAG, "Error executing svm40_read_measured_values_as_integers(): "
                   "%i\n",
                   error);
        }
        else
        {
            s_voc_index = (voc_index);
            s_relative_humidity = (humidity);
            s_temperature = (temperature);
        }
    }

    error = svm40_stop_measurement();
    if (error)
    {
        ESP_LOGE(TAG, "Error executing svm40_stop_measurement(): %i\n", error);
    }
    vTaskDelete(NULL);
}

void voc_index_init()
{
    /* Create and run task to read sensor values and store read sensor values on
    static variables (s_*) */
    xTaskCreate(voc_index_task, "voc task", 1024 * 4, NULL, 5, NULL);
}

void voc_index_get_voc(int16_t *voc_index)
{
    *voc_index = s_voc_index;
}

void voc_index_get_rhumidity(int16_t *rhumidity)
{
    *rhumidity = s_relative_humidity;
}

void voc_index_get_temperature(int16_t *temperature)
{
    *temperature = s_temperature;
}
