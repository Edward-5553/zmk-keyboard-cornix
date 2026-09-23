/* SPDX-License-Identifier: MIT
 * Based on englmaxi/zmk-dongle-display. See ../UPSTREAM.md.
 */
#include <zephyr/kernel.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include "bongo_cat.h"
#include "salary_cat_images.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
#define TYPING_IDLE_MS 3000
#define ACTIVITY_POLL_MS 20

/* Key events and LVGL run on different threads. Only timestamps cross that boundary. */
static struct k_spinlock activity_lock;
static struct {
    bool seen_press;
    int64_t last_press;
} activity;
static lv_timer_t *activity_timer;

static void set_animation(struct zmk_widget_bongo_cat *widget, bool typing) {
    uint8_t state = typing ? 1 : 0;
    if (widget->animation_state == state) {
        return;
    }
    widget->animation_state = state;
    const lv_image_dsc_t **frames = state ? salary_error_frames : salary_idle_frames;
    uint16_t count = state ? SALARY_ERROR_COUNT : SALARY_IDLE_COUNT;
    uint32_t frame_ms = 100;
    lv_animimg_set_src(widget->obj, (const void **)frames, count);
    lv_animimg_set_duration(widget->obj, count * frame_ms);
    // Both states loop; repeated activity updates must not restart playback.
    lv_animimg_set_repeat_count(widget->obj, LV_ANIM_REPEAT_INFINITE);
    lv_animimg_start(widget->obj);
}

static int typing_activity_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (!ev || !ev->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    k_spinlock_key_t key = k_spin_lock(&activity_lock);
    int64_t now = k_uptime_get();
    activity.last_press = now;
    activity.seen_press = true;
    k_spin_unlock(&activity_lock, key);
    return ZMK_EV_EVENT_BUBBLE;
}

static bool typing_active(void) {
    k_spinlock_key_t key = k_spin_lock(&activity_lock);
    int64_t now = k_uptime_get();
    /* The first press activates the animation; every press extends its deadline. */
    bool active = activity.seen_press && now - activity.last_press < TYPING_IDLE_MS;
    k_spin_unlock(&activity_lock, key);
    return active;
}

static void activity_timer_cb(lv_timer_t *timer) {
    (void)timer;
    bool typing = typing_active();
    struct zmk_widget_bongo_cat *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_animation(widget, typing);
    }
}

ZMK_LISTENER(widget_bongo_cat, typing_activity_listener);
ZMK_SUBSCRIPTION(widget_bongo_cat, zmk_position_state_changed);

int zmk_widget_bongo_cat_init(struct zmk_widget_bongo_cat *widget, lv_obj_t *parent) {
    widget->obj = lv_animimg_create(parent);
    widget->animation_state = UINT8_MAX;
    lv_obj_center(widget->obj);
    // Initialize immediately, including when the screen starts at uptime zero.
    set_animation(widget, typing_active());
    sys_slist_append(&widgets, &widget->node);
    if (!activity_timer) {
        /* LVGL timers serialize all animation changes with the display thread. */
        activity_timer = lv_timer_create(activity_timer_cb, ACTIVITY_POLL_MS, NULL);
    }
    return 0;
}

lv_obj_t *zmk_widget_bongo_cat_obj(struct zmk_widget_bongo_cat *widget) {
    return widget->obj;
}
