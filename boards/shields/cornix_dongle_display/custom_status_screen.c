/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "custom_status_screen.h"
#include "portrait_display.h"
#include "portrait_layout.h"
#include "widgets/battery_status.h"
#include "widgets/modifiers.h"
#include "widgets/bongo_cat.h"
#include "widgets/layer_status.h"
#include "widgets/output_status.h"
#include "widgets/hid_indicators.h"
#include "widgets/wpm_status.h"
#include "widgets/compact_fonts.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static struct zmk_widget_output_status output_status_widget;

#if IS_ENABLED(CONFIG_ZMK_BATTERY)
static struct zmk_widget_dongle_battery_status dongle_battery_status_widget;
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_LAYER)
static struct zmk_widget_layer_status layer_status_widget;
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_MODIFIERS)
static struct zmk_widget_modifiers modifiers_widget;
#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
static struct zmk_widget_hid_indicators hid_indicators_widget;
#endif

#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_BONGO_CAT)
static struct zmk_widget_bongo_cat bongo_cat_widget;
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_WPM)
static struct zmk_widget_wpm_status wpm_status_widget;
#endif

lv_style_t global_style;

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen;

    if (cornix_portrait_display_init() != 0) {
        return NULL;
    }

    screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_style_init(&global_style);
    lv_style_set_bg_color(&global_style, lv_color_black());
    lv_style_set_bg_opa(&global_style, LV_OPA_COVER);
    lv_style_set_text_color(&global_style, lv_color_white());
    lv_style_set_text_font(&global_style, &cornix_font_small);
    lv_style_set_text_letter_space(&global_style, 1);
    lv_style_set_text_line_space(&global_style, 0);
    lv_obj_add_style(screen, &global_style, LV_PART_MAIN);

    zmk_widget_output_status_init(&output_status_widget, screen);
    lv_obj_set_pos(zmk_widget_output_status_obj(&output_status_widget), CORNIX_OUTPUT_X, CORNIX_HEADER_Y);

    const int battery_extra = IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY) ?
                              CORNIX_BATTERY_ROW_HEIGHT : 0;

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_WPM)
    zmk_widget_wpm_status_init(&wpm_status_widget, screen);
    lv_obj_set_pos(zmk_widget_wpm_status_obj(&wpm_status_widget), 2, CORNIX_ANIMATION_Y + battery_extra);
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_BONGO_CAT)
    zmk_widget_bongo_cat_init(&bongo_cat_widget, screen);
    lv_obj_set_pos(zmk_widget_bongo_cat_obj(&bongo_cat_widget), 0, CORNIX_CAT_Y + battery_extra / 2);
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_MODIFIERS)
    zmk_widget_modifiers_init(&modifiers_widget, screen);
    lv_obj_set_pos(zmk_widget_modifiers_obj(&modifiers_widget), 1, CORNIX_MODIFIERS_Y);
#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
    zmk_widget_hid_indicators_init(&hid_indicators_widget, screen);
    lv_obj_align(zmk_widget_hid_indicators_obj(&hid_indicators_widget), LV_ALIGN_TOP_MID, 0, CORNIX_LOCK_Y);
#endif
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_LAYER) && !IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_MODIFIERS)
    zmk_widget_layer_status_init(&layer_status_widget, screen);
    lv_obj_set_pos(zmk_widget_layer_status_obj(&layer_status_widget), 2, CORNIX_LAYER_Y);
#endif

#if IS_ENABLED(CONFIG_ZMK_BATTERY)
    zmk_widget_dongle_battery_status_init(&dongle_battery_status_widget, screen);
    lv_obj_set_pos(zmk_widget_dongle_battery_status_obj(&dongle_battery_status_widget), CORNIX_BATTERY_X, CORNIX_BATTERY_Y);
#endif

    return screen;
}
