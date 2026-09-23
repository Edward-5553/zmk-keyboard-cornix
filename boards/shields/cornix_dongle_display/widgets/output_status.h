/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

 #pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_output_status {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_obj_t *symbol;
    lv_obj_t *label;
    uint8_t icon_buffer[LV_CANVAS_BUF_SIZE(9, 10, LV_COLOR_FORMAT_GET_BPP(LV_COLOR_FORMAT_L8), LV_DRAW_BUF_STRIDE_ALIGN)];
};

int zmk_widget_output_status_init(struct zmk_widget_output_status *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_output_status_obj(struct zmk_widget_output_status *widget);
