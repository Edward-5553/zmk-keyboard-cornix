// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define DISPLAY_DIM_MS 60000u
#define DISPLAY_OFF_MS 120000u

enum display_usb { DISPLAY_USB_OFF, DISPLAY_USB_READY, DISPLAY_USB_SUSPENDED };
enum display_hint { DISPLAY_HINT_NONE, DISPLAY_HINT_VOL_UP, DISPLAY_HINT_VOL_DOWN,
                    DISPLAY_HINT_SCROLL_UP, DISPLAY_HINT_SCROLL_DOWN };
struct display_peer {
    bool connected, ready;
    int8_t side, battery; // -1 means unknown, never a synthetic 0% reading.
    bool low;
    uint8_t identity[7];
};
struct display_model {
    struct display_peer peers[2];
    uint8_t wpm_keys[6], wpm_slot;
    uint16_t wpm_counts[10];
    uint32_t wpm_epoch, wpm_last;
    bool wpm_active;
    uint8_t known_identity[2][7];
    bool known[2], has_activity;
    enum display_usb usb;
    enum display_hint hint;
    uint8_t layer;
    uint32_t last_activity, hint_at, pairing_until;
};
void display_model_init(struct display_model *m);
void display_model_connect(struct display_model *m, unsigned peer, const uint8_t identity[7]);
void display_model_disconnect(struct display_model *m, unsigned peer);
void display_model_positions(struct display_model *m, unsigned peer, const uint8_t bits[16]);
void display_model_side(struct display_model *m, unsigned peer, unsigned side);
void display_model_battery(struct display_model *m, unsigned peer, int level);
int display_model_find_side(const struct display_model *m, unsigned side);
bool display_model_working(const struct display_model *m, uint32_t now);
unsigned display_model_brightness(const struct display_model *m, uint32_t now);
unsigned display_model_pairing_seconds(const struct display_model *m, uint32_t now);

void display_model_keyboard(struct display_model *m, uint8_t mods, const uint8_t keys[6], uint32_t now);
unsigned display_model_wpm(const struct display_model *m, uint32_t now);
void display_model_wpm_reset(struct display_model *m);

void display_model_usb(struct display_model *m, enum display_usb usb, uint32_t now);
