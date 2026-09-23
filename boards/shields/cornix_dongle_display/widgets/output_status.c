/* Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT */
#include <zephyr/kernel.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/usb.h>
#include <zmk/endpoints.h>
#if IS_ENABLED(CONFIG_ZMK_BLE)
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/ble.h>
#endif
#include "output_status.h"
#include "compact_fonts.h"
#include "../portrait_layout.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct output_status_state {
    struct zmk_endpoint_instance selected_endpoint;
    int active_profile_index;
    bool active_profile_connected;
    bool usb_is_hid_ready;
};

static struct output_status_state get_state(const zmk_event_t *eh) {
    (void)eh;
    struct output_status_state state = {
        .selected_endpoint = zmk_endpoint_get_selected(),
        .usb_is_hid_ready = zmk_usb_is_hid_ready(),
    };
#if IS_ENABLED(CONFIG_ZMK_BLE)
    state.active_profile_index = zmk_ble_active_profile_index();
    state.active_profile_connected = zmk_ble_active_profile_is_connected();
#endif
    return state;
}

static void set_status_symbol(struct zmk_widget_output_status *widget,
                              struct output_status_state state) {
    static const uint16_t usb[] = {0x10,0x38,0x10,0x96,0x96,0x54,0x38,0x10,0x38,0x38};
    static const uint16_t bt[] = {0x10,0x18,0x54,0x38,0x10,0x38,0x54,0x18,0x10,0};
    static const uint16_t off[] = {0x101,0x82,0x44,0x28,0x10,0x28,0x44,0x82,0x101,0};
    const uint16_t *icon = off;
    if (state.selected_endpoint.transport == ZMK_TRANSPORT_USB && state.usb_is_hid_ready) {
        icon = usb;
        lv_label_set_text(widget->label, "USB");
    } else if (state.selected_endpoint.transport == ZMK_TRANSPORT_BLE &&
               state.active_profile_connected) {
        icon = bt;
        if (state.active_profile_index >= 0 && state.active_profile_index < 9) {
            lv_label_set_text_fmt(widget->label, "BT%d", state.active_profile_index + 1);
        } else {
            lv_label_set_text(widget->label, "BT?");
        }
    } else {
        lv_label_set_text(widget->label, "OFF");
    }
    lv_canvas_fill_bg(widget->symbol, lv_color_black(), LV_OPA_COVER);
    for (int y = 0; y < 10; y++) {
        for (int x = 0; x < 9; x++) {
            if (icon[y] & (1u << (8 - x))) {
                lv_canvas_set_px(widget->symbol, x, y, lv_color_white(), LV_OPA_COVER);
            }
        }
    }
}

static void output_status_update_cb(struct output_status_state state) {
    struct zmk_widget_output_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_status_symbol(widget, state); }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status, struct output_status_state,
                            output_status_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);
#if IS_ENABLED(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
#endif
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);

int zmk_widget_output_status_init(struct zmk_widget_output_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_remove_style_all(widget->obj);
    lv_obj_clear_flag(widget->obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(widget->obj, CORNIX_OUTPUT_WIDTH, CORNIX_HEADER_HEIGHT);
    widget->symbol = lv_canvas_create(widget->obj);
    lv_canvas_set_buffer(widget->symbol, widget->icon_buffer, 9, 10, LV_COLOR_FORMAT_L8);
    lv_obj_set_pos(widget->symbol, 1, 0);
    widget->label = lv_label_create(widget->obj);
    lv_obj_set_style_text_font(widget->label, &cornix_font_small, 0);
    lv_obj_set_style_text_letter_space(widget->label, 1, 0);
    lv_obj_set_size(widget->label, CORNIX_OUTPUT_WIDTH, CORNIX_TEXT_HEIGHT);
    lv_obj_set_pos(widget->label, 0, CORNIX_HEADER_BOTTOM_ROW);
    lv_label_set_long_mode(widget->label, LV_LABEL_LONG_CLIP);
    sys_slist_append(&widgets, &widget->node);
    widget_output_status_init();
    return 0;
}

lv_obj_t *zmk_widget_output_status_obj(struct zmk_widget_output_status *widget) {
    return widget->obj;
}
