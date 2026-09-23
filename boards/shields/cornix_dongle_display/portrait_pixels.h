/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Rotate an LVGL I1 rectangle counterclockwise into SH1106 vertical, LSB-first pages.
 * Width is a multiple of eight, so the rotated rectangle contains whole pages.
 * The caller skips LVGL's eight-byte palette and supplies the actual byte stride.
 * LVGL I1 uses 0=black, 1=white. Zephyr MONO10 uses 1=black, 0=white;
 * MONO01 uses 0=black, 1=white. The SH1106 driver handles hardware inversion. */
static inline void cornix_rotate_mono(const uint8_t *src, size_t stride, uint16_t width,
                                     uint16_t height, bool mono10, uint8_t *dst) {
    memset(dst, mono10 ? 0xff : 0x00, (size_t)width * height / 8);
    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t x = 0; x < width; x++) {
            if (src[y * stride + x / 8] & (0x80 >> (x % 8))) {
                uint16_t rotated_y = width - 1 - x;
                size_t offset = (rotated_y / 8) * height + y;
                uint8_t bit = 1u << (rotated_y % 8);
                if (mono10) {
                    dst[offset] &= (uint8_t)~bit;
                } else {
                    dst[offset] |= bit;
                }
            }
        }
    }
}
