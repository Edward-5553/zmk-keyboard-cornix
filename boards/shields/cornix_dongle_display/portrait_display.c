/* SPDX-License-Identifier: MIT */
#include <errno.h>
#include <lvgl.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
#include "portrait_display.h"
#include "portrait_layout.h"
#include "portrait_pixels.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static const struct device *panel = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
/* Flushes are synchronous on the dedicated display queue. No heap or key-thread I/O. */
static uint8_t rotated[CORNIX_SCREEN_WIDTH * CORNIX_SCREEN_HEIGHT / 8];

static void portrait_flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels) {
    uint16_t width = area->x2 - area->x1 + 1;
    uint16_t height = area->y2 - area->y1 + 1;
    if (area->x1 < 0 || area->y1 < 0 || area->x2 < area->x1 || area->y2 < area->y1 ||
        area->x2 >= CORNIX_SCREEN_WIDTH ||
        area->y2 >= CORNIX_SCREEN_HEIGHT || (area->x1 % 8) || (width % 8) ||
        !width || !height) {
        LOG_ERR("Invalid portrait display area");
        lv_display_flush_ready(display);
        return;
    }

    struct display_capabilities caps;
    display_get_capabilities(panel, &caps);
    cornix_rotate_mono(pixels + 8, lv_draw_buf_width_to_stride(width, LV_COLOR_FORMAT_I1),
                       width, height, caps.current_pixel_format == PIXEL_FORMAT_MONO10, rotated);
    struct display_buffer_descriptor desc = {
        .buf_size = (size_t)width * height / 8,
        .width = height,
        .pitch = height,
        .height = width,
        .frame_incomplete = !lv_display_flush_is_last(display),
    };
    int err = display_write(panel, area->y1, CORNIX_SCREEN_WIDTH - 1 - area->x2,
                            &desc, rotated);
    if (err) {
        LOG_ERR("Portrait display write failed: %d", err);
    }
    lv_display_flush_ready(display);
}

int cornix_portrait_display_init(void) {
    struct display_capabilities caps;
    display_get_capabilities(panel, &caps);
    if (caps.x_resolution != CORNIX_SCREEN_HEIGHT || caps.y_resolution != CORNIX_SCREEN_WIDTH ||
        !(caps.screen_info & SCREEN_INFO_MONO_VTILED) ||
        (caps.screen_info & (SCREEN_INFO_MONO_MSB_FIRST | SCREEN_INFO_X_ALIGNMENT_WIDTH)) ||
        (caps.current_pixel_format != PIXEL_FORMAT_MONO10 &&
         caps.current_pixel_format != PIXEL_FORMAT_MONO01)) {
        LOG_ERR("Cornix portrait display requires a 128x64 SH1106-compatible panel");
        return -ENOTSUP;
    }
    lv_display_t *display = lv_display_get_default();
    /* The SH1106 has no 90-degree hardware rotation. Merely setting LVGL's
     * rotation would leave Zephyr 4.1's monochrome flush coordinates unrotated.
     * Keep the physical devicetree 128x64 and rotate every dirty rectangle here.
     * Counterclockwise puts the magnetic-mount view 180 degrees from the old UI. */
    lv_display_set_resolution(display, CORNIX_SCREEN_WIDTH, CORNIX_SCREEN_HEIGHT);
    lv_display_set_flush_wait_cb(display, NULL);
    lv_display_set_flush_cb(display, portrait_flush);
    return 0;
}
