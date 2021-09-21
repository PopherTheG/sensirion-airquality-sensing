#ifndef COMPONENTS_CO2_H
#define COMPONENTS_CO2_H

void co2_init();

/**
 * @brief get the CO2 value from CO2 sensor.
 * 
 * @param[out] co2 get the co2 value. No conversion needed. Unit in ppm.
 */ 
void co2_get_co2(uint16_t *co2);

#endif