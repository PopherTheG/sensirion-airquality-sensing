#ifndef COMPONENTS_VOC_INDEX_H
#define COMPONENTS_VOC_INDEX_H

/**
 * @brief Start the task for reading values from VOC sensor and update values on VOC,
 * temperature, and relative humidty.
 */ 
void voc_index_init();

/**
 * @brief Get the VOC index.
 * 
 * @param[out] voc_index returns the raw VOC index value data. You must divide it by 10 to get
 * real value. This is done to have precision in having an int16_t variable. Conterted unit is unitless.
 */ 
void voc_index_get_voc(int16_t *voc_index);

/**
 * @brief Get relative humidity.
 * 
 * @param[out] rhumidity returns raw relative humidity data. You must divide the value by 100 to
 * get real value. This is done to have precision in having an int16_t variable. Converted unit in %.
 */ 
void voc_index_get_rhumidity(int16_t *rhumidity);

/**
 * @brief Get the temperature.
 * 
 * @param[out] temperature returns raw temperature data. You must divide the value by 200 to get
 * real value. This is done to have precision in having an int16_t variable. Converted unit is C.
 */ 
void voc_index_get_temperature(int16_t *temperature);

#endif