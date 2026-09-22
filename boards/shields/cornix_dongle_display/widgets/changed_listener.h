/* SPDX-License-Identifier: MIT */
#pragma once

#include <zephyr/kernel.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>

/* Keep the same thread boundary as ZMK_DISPLAY_WIDGET_LISTENER, but compare
 * semantic state before submitting work. Compare against the latest queued
 * state, not the last rendered state, so coalesced A -> B -> A updates survive.
 * The equality function must compare fields rather than struct padding. */
#define ZMK_DONGLE_DISPLAY_WIDGET_LISTENER(listener, state_type, cb, get_state, equal) \
    K_MUTEX_DEFINE(listener##_mutex);                                                \
    static state_type listener##_state;                                              \
    static state_type listener##_get_local_state(void) {                             \
        k_mutex_lock(&listener##_mutex, K_FOREVER);                                   \
        state_type copy = listener##_state;                                          \
        k_mutex_unlock(&listener##_mutex);                                           \
        return copy;                                                                \
    }                                                                               \
    static void listener##_work_cb(struct k_work *work) {                            \
        (void)work;                                                                 \
        cb(listener##_get_local_state());                                           \
    }                                                                               \
    K_WORK_DEFINE(listener##_work, listener##_work_cb);                               \
    static void listener##_init(void) {                                             \
        k_mutex_lock(&listener##_mutex, K_FOREVER);                                   \
        listener##_state = get_state(NULL);                                          \
        k_mutex_unlock(&listener##_mutex);                                           \
        listener##_work_cb(NULL);                                                   \
    }                                                                               \
    static int listener##_cb(const zmk_event_t *eh) {                                 \
        if (zmk_display_is_initialized()) {                                          \
            k_mutex_lock(&listener##_mutex, K_FOREVER);                               \
            state_type next = get_state(eh);                                         \
            bool changed = !equal(listener##_state, next);                           \
            listener##_state = next;                                                \
            k_mutex_unlock(&listener##_mutex);                                       \
            if (changed) {                                                          \
                k_work_submit_to_queue(zmk_display_work_q(), &listener##_work);       \
            }                                                                       \
        }                                                                           \
        return ZMK_EV_EVENT_BUBBLE;                                                  \
    }                                                                               \
    ZMK_LISTENER(listener, listener##_cb);
