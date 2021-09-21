/*
 * Copyright (c) 2018, Sensirion AG
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Sensirion AG nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "sensirion_i2c_hal.h"
#include "sensirion_common.h"
#include "sensirion_config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_err.h"

/* NOTE: This sensor is set to connect to I2C_NUM_1 I2C bus. */

int16_t sensirion_i2c_hal_select_bus(uint8_t bus_idx)
{
    return 0;
}

void sensirion_i2c_hal_init(void)
{
    // Initialization of I2C bus is implemented in 'main.c'
}

void sensirion_i2c_hal_free(void)
{
    // This is not needed in this implementation.
}

int8_t sensirion_i2c_hal_read(uint8_t address, uint8_t *data, uint16_t count)
{
    esp_err_t err;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_READ, 1);
    i2c_master_read(cmd, data, count, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(I2C_NUM_1, cmd, 200 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    return err;
}

int8_t sensirion_i2c_hal_write(uint8_t address, const uint8_t *data,
                               uint16_t count)
{
    esp_err_t err;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, 1);
    i2c_master_write(cmd, data, count, 1);
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(I2C_NUM_1, cmd, 200 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    return err;
}

void sensirion_i2c_hal_sleep_usec(uint32_t useconds)
{
    vTaskDelay(pdMS_TO_TICKS(useconds / 1000));
}
