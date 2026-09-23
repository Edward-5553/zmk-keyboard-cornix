/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/services/bas.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/physical_layouts.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/usb.h>

#include "battery_status.h"
#include "changed_listener.h"
#include "compact_fonts.h"
#include "../portrait_layout.h"

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY)
    #define SOURCE_OFFSET 1
#else
    #define SOURCE_OFFSET 0
#endif

#ifndef ZMK_SPLIT_BLE_PERIPHERAL_COUNT
#  define ZMK_SPLIT_BLE_PERIPHERAL_COUNT 0
#endif

#define BUFFER_SIZE LV_CANVAS_BUF_SIZE(8, 6, LV_COLOR_FORMAT_GET_BPP(LV_COLOR_FORMAT_L8), LV_DRAW_BUF_STRIDE_ALIGN)

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct battery_state {
    uint8_t source;
    uint8_t level;
    bool usb_present;
    bool valid;
    char side;
};

/* Keep every source in the snapshot: display work may coalesce several events. */
struct battery_snapshot {
    struct battery_state sources[ZMK_SPLIT_BLE_PERIPHERAL_COUNT + SOURCE_OFFSET];
};

struct battery_object {
    lv_obj_t *symbol;
    lv_obj_t *label;
    lv_obj_t *side_label;
    struct battery_state rendered;
} battery_objects[ZMK_SPLIT_BLE_PERIPHERAL_COUNT + SOURCE_OFFSET];

static uint8_t battery_image_buffer[ZMK_SPLIT_BLE_PERIPHERAL_COUNT + SOURCE_OFFSET][BUFFER_SIZE];

static bool battery_state_equal(struct battery_state a, struct battery_state b) {
    return a.source == b.source && a.level == b.level && a.usb_present == b.usb_present &&
           a.valid == b.valid && a.side == b.side;
}

static bool battery_snapshot_equal(struct battery_snapshot a, struct battery_snapshot b) {
    for (int i = 0; i < ARRAY_SIZE(a.sources); i++) {
        if (!battery_state_equal(a.sources[i], b.sources[i])) {
            return false;
        }
    }
    return true;
}

static void draw_battery(lv_obj_t *canvas, uint8_t level, bool usb_present) {
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);

    unsigned fill = ((level > 100 ? 100 : level) * 3 + 50) / 100;
    for (int y = 0; y < 6; y++) {
        for (int x = 0; x < 8; x++) {
            bool border = (x < 7 && (y == 0 || y == 5 || x == 0 || x == 6)) ||
                          (x == 7 && (y == 2 || y == 3));
            bool charge = x >= 2 && x < 2 + fill && (y == 2 || y == 3);
            if (border || charge || (usb_present && x == 4 && y == 1)) {
                lv_canvas_set_px(canvas, x, y, lv_color_white(), LV_OPA_COVER);
            }
        }
    }
}

static void set_battery_symbol(lv_obj_t *widget, int row, struct battery_state state) {
    (void)widget;
    struct battery_object *object = &battery_objects[row];
    if (battery_state_equal(object->rendered, state)) {
        return;
    }
    LOG_DBG("source: %d, level: %d, usb: %d", state.source, state.level, state.usb_present);
    lv_obj_t *symbol = object->symbol;
    lv_obj_t *label = object->label;

    draw_battery(symbol, state.level, state.usb_present);
    if (state.valid) {
        lv_label_set_text_fmt(label, "%u%%", state.level);
    } else {
        lv_label_set_text_fmt(label, "--%%");
    }
    object->rendered = state;
}

void battery_status_update_cb(struct battery_snapshot snapshot) {
    /* Source IDs follow pairing order, not handedness. Keep visible L/R rows
     * fixed and leave unknown readings blank rather than assigning the wrong side. */
    struct battery_state rows[ARRAY_SIZE(snapshot.sources)] = {0};
    for (int i = 0; i < ARRAY_SIZE(rows); i++) {
        rows[i].side = i < SOURCE_OFFSET ? 'D' : (i == SOURCE_OFFSET ? 'L' : 'R');
    }
    for (int i = 0; i < ARRAY_SIZE(snapshot.sources); i++) {
        struct battery_state state = snapshot.sources[i];
        int row = state.side == 'D' && SOURCE_OFFSET ? 0 :
                  state.side == 'L' ? SOURCE_OFFSET :
                  state.side == 'R' ? SOURCE_OFFSET + 1 : -1;
        if (row >= 0 && row < ARRAY_SIZE(rows)) {
            rows[row] = state;
        }
    }
    struct zmk_widget_dongle_battery_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        for (int i = 0; i < ARRAY_SIZE(rows); i++) {
            set_battery_symbol(widget->obj, i, rows[i]);
        }
    }
}

static struct battery_state peripheral_battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *ev = as_zmk_peripheral_battery_state_changed(eh);
    return (struct battery_state){
        .source = ev->source + SOURCE_OFFSET,
        .level = ev->state_of_charge,
        .valid = true,
    };
}

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY)
static struct battery_state central_battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = eh ? as_zmk_battery_state_changed(eh) : NULL;
    return (struct battery_state) {
        .source = 0,
        .side = 'D',
        .valid = true,
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
    };
}

#endif

static char side_for_position(uint32_t position) {
    const struct zmk_physical_layout *const *layouts;
    size_t count = zmk_physical_layouts_get_list(&layouts);
    int selected = zmk_physical_layouts_get_selected();
    if (selected < 0 || (size_t)selected >= count || !layouts[selected]->keys ||
        position >= layouts[selected]->keys_len) {
        return '?';
    }

    /* Cornix coordinates (1/100 key unit): left x <= 600, right x >= 750.
     * Use physical positions, so remapping keys in Studio does not change sides. */
    int16_t x = layouts[selected]->keys[position].x;
    return x <= 600 ? 'L' : (x >= 750 ? 'R' : '?');
}

static struct battery_snapshot battery_status_get_state(const zmk_event_t *eh) {
    static struct battery_snapshot snapshot;
    const struct zmk_position_state_changed *position =
        eh ? as_zmk_position_state_changed(eh) : NULL;
    if (position) {
        if (position->state && position->source < ZMK_SPLIT_BLE_PERIPHERAL_COUNT) {
            char side = side_for_position(position->position);
            if (side != '?') {
                snapshot.sources[position->source + SOURCE_OFFSET].side = side;
            }
        }
    } else if (eh && as_zmk_peripheral_battery_state_changed(eh)) {
        if (as_zmk_peripheral_battery_state_changed(eh)->source >=
            ZMK_SPLIT_BLE_PERIPHERAL_COUNT) {
            return snapshot;
        }
        struct battery_state state = peripheral_battery_status_get_state(eh);
        if (state.source < ARRAY_SIZE(snapshot.sources)) {
            state.side = snapshot.sources[state.source].side;
            snapshot.sources[state.source] = state;
        }
    }
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY)
    else {
        snapshot.sources[0] = central_battery_status_get_state(eh);
    }
#endif
    return snapshot;
}

ZMK_DONGLE_DISPLAY_WIDGET_LISTENER(widget_dongle_battery_status, struct battery_snapshot,
                                  battery_status_update_cb, battery_status_get_state,
                                  battery_snapshot_equal)

ZMK_SUBSCRIPTION(widget_dongle_battery_status, zmk_peripheral_battery_state_changed);
ZMK_SUBSCRIPTION(widget_dongle_battery_status, zmk_position_state_changed);

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY)
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

ZMK_SUBSCRIPTION(widget_dongle_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_dongle_battery_status, zmk_usb_conn_state_changed);
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
#endif /* !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) */
#endif /* IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY) */

int zmk_widget_dongle_battery_status_init(struct zmk_widget_dongle_battery_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);

    lv_obj_remove_style_all(widget->obj);
    lv_obj_clear_flag(widget->obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(widget->obj, CORNIX_BATTERY_WIDTH,
                    (ZMK_SPLIT_BLE_PERIPHERAL_COUNT + SOURCE_OFFSET - 1) *
                        CORNIX_BATTERY_ROW_HEIGHT + CORNIX_TEXT_HEIGHT);

    for (int i = 0; i < ZMK_SPLIT_BLE_PERIPHERAL_COUNT + SOURCE_OFFSET; i++) {
        lv_obj_t *image_canvas = lv_canvas_create(widget->obj);
        lv_obj_t *battery_label = lv_label_create(widget->obj);
        lv_obj_t *side_label = lv_label_create(widget->obj);
        /* L/R always occupy the top two rows; optional dongle battery goes last. */
        int row = i < SOURCE_OFFSET ? ZMK_SPLIT_BLE_PERIPHERAL_COUNT : i - SOURCE_OFFSET;
        int y = row * CORNIX_BATTERY_ROW_HEIGHT;
        lv_canvas_set_buffer(image_canvas, battery_image_buffer[i], 8, 6, LV_COLOR_FORMAT_L8);
        lv_obj_set_pos(image_canvas, CORNIX_BATTERY_ICON_X, y);
        lv_obj_set_pos(side_label, 0, y);
        lv_obj_set_size(side_label, 4, CORNIX_TEXT_HEIGHT);
        lv_obj_set_style_text_font(side_label, &cornix_font_small, 0);
        lv_label_set_text_fmt(side_label, "%c", i < SOURCE_OFFSET ? 'D' : (row == 0 ? 'L' : 'R'));
        lv_obj_set_pos(battery_label, CORNIX_BATTERY_LABEL_X, y);
        lv_obj_set_size(battery_label, CORNIX_BATTERY_LABEL_WIDTH, CORNIX_TEXT_HEIGHT);
        lv_obj_set_style_text_font(battery_label, &cornix_font_small, 0);
        lv_obj_set_style_text_letter_space(battery_label, 1, 0);
        lv_obj_set_style_text_align(battery_label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_long_mode(battery_label, LV_LABEL_LONG_CLIP);

        battery_objects[i] = (struct battery_object){
            .symbol = image_canvas,
            .label = battery_label,
            .side_label = side_label,
        };
    }

    sys_slist_append(&widgets, &widget->node);

    widget_dongle_battery_status_init();

    return 0;
}

lv_obj_t *zmk_widget_dongle_battery_status_obj(struct zmk_widget_dongle_battery_status *widget) {
    return widget->obj;
}
