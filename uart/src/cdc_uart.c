/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <pico/stdlib.h>

#include "picoprobe_config.h"
#include "tusb.h"

static uint8_t tx_buf[CFG_TUD_CDC_TX_BUFSIZE];
static uint8_t rx_buf[CFG_TUD_CDC_RX_BUFSIZE];

static uint8_t tx2_buf[CFG_TUD_CDC_TX_BUFSIZE];
static uint8_t rx2_buf[CFG_TUD_CDC_RX_BUFSIZE];

void cdc_uart_init(void) {
    gpio_set_function(PICOPROBE_UART_TX, GPIO_FUNC_UART);
    gpio_set_function(PICOPROBE_UART_RX, GPIO_FUNC_UART);
    gpio_set_pulls(PICOPROBE_UART_TX, 1, 0);
    gpio_set_pulls(PICOPROBE_UART_RX, 1, 0);
    uart_init(PICOPROBE_UART_INTERFACE, PICOPROBE_UART_BAUDRATE);
    gpio_set_function(PICOPROBE_UART2_TX, GPIO_FUNC_UART);
    gpio_set_function(PICOPROBE_UART2_RX, GPIO_FUNC_UART);
    gpio_set_pulls(PICOPROBE_UART2_TX, 1, 0);
    gpio_set_pulls(PICOPROBE_UART2_RX, 1, 0);
    uart_init(PICOPROBE_UART2_INTERFACE, PICOPROBE_UART2_BAUDRATE);
}

void cdc_task(void) {
    static int was_connected = 0;
    uint rx_len = 0;
    uint rx2_len = 0;

    // Consume uart fifo regardless even if not connected
    while (uart_is_readable(PICOPROBE_UART_INTERFACE) && (rx_len < sizeof(rx_buf))) {
        rx_buf[rx_len++] = uart_getc(PICOPROBE_UART_INTERFACE);
    }
    // Consume uart fifo regardless even if not connected
    while (uart_is_readable(PICOPROBE_UART2_INTERFACE) && (rx2_len < sizeof(rx2_buf))) {
        rx2_buf[rx2_len++] = uart_getc(PICOPROBE_UART2_INTERFACE);
    }

    if (tud_ready()) {
        was_connected = 1;
        int written = 0;
        /* Implicit overflow if we don't write all the bytes to the host.
         * Also throw away bytes if we can't write... */
        if (rx_len) {
            written = MIN(tud_cdc_write_available(), rx_len);
            if (written > 0) {
                tud_cdc_write(rx_buf, written);
                tud_cdc_write_flush();
            }
        }

        /* Reading from a firehose and writing to a FIFO. */
        size_t watermark = MIN(tud_cdc_available(), sizeof(tx_buf));
        if (watermark > 0) {
            size_t tx_len;
            /* Batch up to half a FIFO of data - don't clog up on RX */
            watermark = MIN(watermark, 16);
            tx_len = tud_cdc_read(tx_buf, watermark);
            uart_write_blocking(PICOPROBE_UART_INTERFACE, tx_buf, tx_len);
        }
        written = 0;
        /* Implicit overflow if we don't write all the bytes to the host.
         * Also throw away bytes if we can't write... */
        if (rx2_len) {
            written = MIN(tud_cdc_n_write_available(1), rx2_len);
            if (written > 0) {
                tud_cdc_n_write(1, rx2_buf, written);
                tud_cdc_n_write_flush(1);
            }
        }

        /* Reading from a firehose and writing to a FIFO. */
        watermark = MIN(tud_cdc_n_available(1), sizeof(tx2_buf));
        if (watermark > 0) {
            size_t tx_len;
            /* Batch up to half a FIFO of data - don't clog up on RX */
            watermark = MIN(watermark, 16);
            tx_len = tud_cdc_n_read(1, tx2_buf, watermark);
            uart_write_blocking(PICOPROBE_UART2_INTERFACE, tx2_buf, tx_len);
        }
    } else if (was_connected) {
        tud_cdc_write_clear();
        tud_cdc_n_write_clear(1);
        was_connected = 0;
    }
}

void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const* line_coding) {
    /* Set the tick thread interval to the amount of time it takes to
     * fill up half a FIFO. Millis is too coarse for integer divide.
     */
    uint32_t micros = (1000 * 1000 * 16 * 10) / MAX(line_coding->bit_rate, 1);

    uart_deinit(PICOPROBE_UART_INTERFACE);
    uart_deinit(PICOPROBE_UART2_INTERFACE);
    tud_cdc_write_clear();
    tud_cdc_read_flush();
    uart_init(PICOPROBE_UART_INTERFACE, line_coding->bit_rate);
    uart_init(PICOPROBE_UART2_INTERFACE, line_coding->bit_rate);
}

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
}
