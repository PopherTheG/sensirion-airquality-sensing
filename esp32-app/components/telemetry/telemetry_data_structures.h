#ifndef COMPONENTS_TELEMETRY_DATA_STRUCTURES_H
#define COMPONENTS_TELEMETRY_DATA_STRUCTURES_H

#include <stdint.h>
#include "telemetry_enums.h"

typedef struct
{
    telemetry_type_t type; // 16 bits (?)
    telemetry_device_type_t device_type; // 16 bits (?)
    uint64_t serial;
    uint32_t timestamp;
    int16_t voc;
    int16_t temperature;
    int16_t rhumidity;
    uint16_t co2;
    uint16_t pm2p5;
    uint16_t pm10p0;
} __attribute__((packed)) telemetry_airquality_t;

#endif