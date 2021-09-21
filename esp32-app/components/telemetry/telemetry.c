#include "telemetry.h"
#include "telemetry_data_structures.h"
#include "telemetry_enums.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "voc_index.h"
#include "co2.h"
#include "particulate_matter.h"

#define TAG "telemetry.c"

static void telemetry_send_airqualitydata_task(void *pvParameters);

static void telemetry_send_airqualitydata_task(void *pvParameters)
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(10000));
        telemetry_airquality_t data;

        int16_t raw_voc;
        int16_t raw_temp;
        int16_t raw_hum;
        uint16_t raw_co2;
        uint16_t raw_pm2p5;
        uint16_t raw_pm10p0;

        data.type = TELEMETRY_TYPE_AIRQUALITY;
        data.device_type = TELEMETRY_DEVICE_TYPE_ECM;
        data.serial = 0x1122334455667788;
        data.timestamp = 0xDEADBEEF;

        voc_index_get_voc(&raw_voc);
        data.voc = raw_voc;
        voc_index_get_temperature(&raw_temp);
        data.temperature = raw_temp;
        voc_index_get_rhumidity(&raw_hum);
        data.rhumidity = raw_hum;
        co2_get_co2(&raw_co2);
        data.co2 = raw_co2;
        particulate_matter_get_pm2p5(&raw_pm2p5);
        data.pm2p5 = raw_pm2p5;
        particulate_matter_get_pm10p0(&raw_pm10p0);
        data.pm10p0 = raw_pm10p0;

        // Logging on serial the packed air quality data.
        size_t data_size = sizeof(data);
        ESP_LOGI(TAG, "%zu", data_size);
        char *data_ptr = (char *)&data;
        printf("telemetry.c, airquality telemetry payload: ");
        while (data_size--)
        {
            printf("%x", *data_ptr++);
        }
        printf("\n");
    }
}

void telemetry_init()
{
    xTaskCreate(telemetry_send_airqualitydata_task, "airqualirt sending task", 1024 * 2, NULL, 5, NULL);
}