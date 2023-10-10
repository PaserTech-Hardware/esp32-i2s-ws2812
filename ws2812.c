/** MIT licence

 Copyright (C) 2019 by Vu Nam https://github.com/vunam https://studiokoda.com

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights to
 use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 of the Software, and to permit persons to whom the Software is furnished to do
 so, subject to the following conditions: The above copyright notice and this
 permission notice shall be included in all copies or substantial portions of
 the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO
 EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
 OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 DEALINGS IN THE SOFTWARE.

*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/i2s.h"
#include "ws2812.h"
#include "esp_console.h"
#include "esp_log.h"
#include "argtable3/argtable3.h"

static const char TAG[] = "ws2812";

i2s_config_t i2s_config = {
    .mode = I2S_MODE_MASTER | I2S_MODE_TX,
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = 16,
    .communication_format = I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .intr_alloc_flags = 0,
    .dma_buf_count = 4,
    .use_apll = false,
};

i2s_pin_config_t pin_config = {.bck_io_num = -1,
                               .ws_io_num = -1,
                               .data_out_num = I2S_DO_IO,
                               .data_in_num = -1};

static uint8_t out_buffer[LED_NUMBER * PIXEL_SIZE] = {0};
static uint8_t off_buffer[ZERO_BUFFER] = {0};
static uint16_t size_buffer;

static const uint16_t bitpatterns[4] = {0x88, 0x8e, 0xe8, 0xee};

void ws2812_init() {
  size_buffer = LED_NUMBER * PIXEL_SIZE;

  i2s_config.dma_buf_len = size_buffer;

  i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM, &pin_config);
}

void ws2812_update(ws2812_pixel_t *pixels) {
  size_t bytes_written = 0;

  for (uint16_t i = 0; i < LED_NUMBER; i++) {
    int loc = i * PIXEL_SIZE;

    out_buffer[loc] = bitpatterns[pixels[i].green >> 6 & 0x03];
    out_buffer[loc + 1] = bitpatterns[pixels[i].green >> 4 & 0x03];
    out_buffer[loc + 2] = bitpatterns[pixels[i].green >> 2 & 0x03];
    out_buffer[loc + 3] = bitpatterns[pixels[i].green & 0x03];

    out_buffer[loc + 4] = bitpatterns[pixels[i].red >> 6 & 0x03];
    out_buffer[loc + 5] = bitpatterns[pixels[i].red >> 4 & 0x03];
    out_buffer[loc + 6] = bitpatterns[pixels[i].red >> 2 & 0x03];
    out_buffer[loc + 7] = bitpatterns[pixels[i].red & 0x03];

    out_buffer[loc + 8] = bitpatterns[pixels[i].blue >> 6 & 0x03];
    out_buffer[loc + 9] = bitpatterns[pixels[i].blue >> 4 & 0x03];
    out_buffer[loc + 10] = bitpatterns[pixels[i].blue >> 2 & 0x03];
    out_buffer[loc + 11] = bitpatterns[pixels[i].blue & 0x03];
  }

  i2s_write(I2S_NUM, out_buffer, size_buffer, &bytes_written, portMAX_DELAY);
  i2s_write(I2S_NUM, off_buffer, ZERO_BUFFER, &bytes_written, portMAX_DELAY);
  vTaskDelay(pdMS_TO_TICKS(10));
  i2s_zero_dma_buffer(I2S_NUM);
}

static struct {
    struct arg_str *color;
    struct arg_int *pos;
    struct arg_end *end;
} g_ws2812_args;

ws2812_pixel_t g_ws2812_pixels[LED_NUMBER] = {0};

static int ws2812_handler(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &g_ws2812_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, g_ws2812_args.end, argv[0]);
        return 1;
    }

    int pos = 0;
    if(g_ws2812_args.pos->count > 0) {
        pos = g_ws2812_args.pos->ival[0];
    }
    if(pos < 0 || pos >= LED_NUMBER) {
        ESP_LOGW(TAG, "Invalid argument: pos must be between 0 and %d", LED_NUMBER - 1);
        return 1;
    }

    char *endptr;
    uint32_t color = strtol(g_ws2812_args.color->sval[0], &endptr, 16);
    if(*endptr != '\0') {
        ESP_LOGW(TAG, "Invalid argument: color must be valid hex!");
        return 1;
    }
    g_ws2812_pixels[pos].red = color >> 16;
    g_ws2812_pixels[pos].green = color >> 8;
    g_ws2812_pixels[pos].blue = color;

    ws2812_update(g_ws2812_pixels);
    return 0;
}

void register_ws2812(void)
{
    g_ws2812_args.color = arg_str1(NULL, NULL, "<color>", "The color in hex format. Example: ff0000");
    g_ws2812_args.pos = arg_int0(NULL, "pos", "<pos>", "The position of the led. Default is 0");
    g_ws2812_args.end = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command = "ws2812",
        .help = "Set the ws2812 color",
        .hint = NULL,
        .func = &ws2812_handler,
        .argtable = &g_ws2812_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}
