/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Rotate an LVGL I1 rectangle clockwise into SH1106 vertical, LSB-first pages.
 * Width is a multiple of eight, so the rotated rectangle contains whole pages.
 * The caller skips LVGL's eight-byte palette and supplies the actual byte stride.
 * Pixel polarity matches Zephyr 4.1's lvgl_display_mono.c. */
static inline void cornix_rotate_mono(const uint8_t *src, size_t stride, uint16_t width,
                                     uint16_t height, bool mono10, uint8_t *dst) {
    memset(dst, mono10 ? 0x00 : 0xff, (size_t)width * height / 8);
    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t x = 0; x < width; x++) {
            if (src[y * stride + x / 8] & (0x80 >> (x % 8))) {
                size_t offset = (x / 8) * height + (height - 1 - y);
                uint8_t bit = 1u << (x % 8);
                if (mono10) {
                    dst[offset] |= bit;
                } else {
                    dst[offset] &= (uint8_t)~bit;
                }
            }
        }
    }
}
