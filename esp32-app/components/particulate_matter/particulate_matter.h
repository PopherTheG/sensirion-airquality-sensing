#ifndef COMPONENTS_PARTICULATE_MATTER_H
#define COMPONENTS_PARTICULATE_MATTER_H

/**
 * @brief Start the task for reading values from particulate matter sensor and update 
 * values on particulate matter.
 */ 
void particulate_matter_init();

/**
 * @brief Get pm10.
 * 
 * @param[out] pm10p0 returns pm10. Must divide by 1000 to get real value. Converted unit in microgram/meter cube. 
 */ 
void particulate_matter_get_pm10p0(uint16_t *pm10p0);

/**
 * @brief Get pm2.5.
 * 
 * @param[out] pm2p5 returns pm2.5. Must divide by 1000 to get real value. Converted unit in microgram/meter cube. 
 */ 
void particulate_matter_get_pm2p5(uint16_t *pm2p5);

#endif