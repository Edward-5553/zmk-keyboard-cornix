// SPDX-License-Identifier: MIT
#pragma once
#include "display_model.h"
void display_status_init(void);
void display_status_connect(unsigned peer, const uint8_t identity[7]);
void display_status_ready(unsigned peer);
void display_status_disconnect(unsigned peer);
void display_status_positions(unsigned peer, const uint8_t bits[16]);
void display_status_side(unsigned peer, unsigned side);
void display_status_battery(unsigned peer, int battery);
void display_status_pairing(uint32_t until);
void display_status_usb(enum display_usb usb);
void display_status_layer(uint8_t layer);
void display_status_activity(enum display_hint hint);
void display_status_snapshot(struct display_model *out);
void display_start(void);

void display_status_keyboard(uint8_t mods,const uint8_t keys[6]);
