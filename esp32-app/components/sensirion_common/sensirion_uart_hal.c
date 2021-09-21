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

#include "sensirion_uart_hal.h"
#include "sensirion_common.h"
#include "sensirion_config.h"

#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"

#define SENSIRION_UART UART_NUM_1
#define TXD_PIN_UART1 GPIO_NUM_25
#define RXD_PIN_UART1 GPIO_NUM_26
#define RX_BUF_SIZE (1024 * 1)
// #define TX_BUF_SIZE (1024 * 1)
#define TAG "sensirion_uart_hal.c"

int16_t sensirion_uart_hal_init()
{
    /* NOTE: UART initialization is assumed to be done in 'main.c' */
    return 0;
}

int16_t sensirion_uart_hal_free()
{
    int16_t err;
    err = uart_driver_delete(SENSIRION_UART);
    return err;
}

int16_t sensirion_uart_hal_tx(uint16_t data_len, const uint8_t *data)
{
    return uart_write_bytes(SENSIRION_UART, (const char *)data, data_len);
}

int16_t sensirion_uart_hal_rx(uint16_t max_data_len, uint8_t *data)
{
    return uart_read_bytes(SENSIRION_UART, (uint8_t *)data, max_data_len, pdMS_TO_TICKS(200));
}

void sensirion_uart_hal_sleep_usec(uint32_t useconds)
{
}
