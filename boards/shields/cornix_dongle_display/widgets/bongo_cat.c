/* SPDX-License-Identifier: MIT
 * Based on englmaxi/zmk-dongle-display. See ../UPSTREAM.md.
 */
#include <zephyr/kernel.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/wpm.h>
#include "bongo_cat.h"
#include "salary_cat_images.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
struct bongo_cat_wpm_status_state { uint8_t wpm; };

static void set_animation(struct zmk_widget_bongo_cat *widget, uint8_t wpm) {
    uint8_t state = wpm < 5 ? 0 : wpm < 30 ? 1 : wpm < 70 ? 2 : 3;
    if (widget->animation_state == state) {
        return;
    }
    widget->animation_state = state;
    const lv_image_dsc_t **frames = salary_sleep_frames;
    uint16_t count = SALARY_SLEEP_COUNT;
    uint32_t frame_ms = 100;
    if (state == 1 || state == 2) {
        frames = salary_work_frames;
        count = SALARY_WORK_COUNT;
        frame_ms = state == 1 ? 200 : 100;
    } else if (state == 3) {
        frames = salary_snack_frames;
        count = SALARY_SNACK_COUNT;
    }
    lv_animimg_set_src(widget->obj, (const void **)frames, count);
    lv_animimg_set_duration(widget->obj, count * frame_ms);
    // Idle is one still frame and finishes once, with no repeating animation.
    lv_animimg_set_repeat_count(widget->obj, state == 0 ? 0 : LV_ANIM_REPEAT_INFINITE);
    lv_animimg_start(widget->obj);
}

static struct bongo_cat_wpm_status_state bongo_cat_wpm_status_get_state(const zmk_event_t *eh) {
    const struct zmk_wpm_state_changed *ev = as_zmk_wpm_state_changed(eh);
    return (struct bongo_cat_wpm_status_state){ .wpm = ev ? ev->state : zmk_wpm_get_state() };
}

static void bongo_cat_wpm_status_update_cb(struct bongo_cat_wpm_status_state state) {
    struct zmk_widget_bongo_cat *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_animation(widget, state.wpm);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_bongo_cat, struct bongo_cat_wpm_status_state,
                           bongo_cat_wpm_status_update_cb, bongo_cat_wpm_status_get_state)
ZMK_SUBSCRIPTION(widget_bongo_cat, zmk_wpm_state_changed);

int zmk_widget_bongo_cat_init(struct zmk_widget_bongo_cat *widget, lv_obj_t *parent) {
    widget->obj = lv_animimg_create(parent);
    widget->animation_state = UINT8_MAX;
    lv_obj_center(widget->obj);
    // Initialize immediately, including when the screen starts at uptime zero.
    set_animation(widget, zmk_wpm_get_state());
    sys_slist_append(&widgets, &widget->node);
    widget_bongo_cat_init();
    return 0;
}

lv_obj_t *zmk_widget_bongo_cat_obj(struct zmk_widget_bongo_cat *widget) {
    return widget->obj;
}
