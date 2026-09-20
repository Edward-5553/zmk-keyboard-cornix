// SPDX-License-Identifier: MIT
#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#define DISPLAY_WIDTH 466
#define DISPLAY_HEIGHT 466
#define DISPLAY_BUFFER_BYTES (DISPLAY_WIDTH * 16 * 2)
esp_err_t display_hw_init(void);
esp_err_t display_hw_flush(int x1,int y1,int x2,int y2,const uint8_t *pixels);
esp_err_t display_hw_brightness(unsigned percent);
esp_err_t display_hw_enabled(bool enabled);
esp_err_t display_hw_orientation_init(void);

void display_hw_audio_off(void);
