// SPDX-License-Identifier: MIT
#pragma once
#include "engine.h"
enum input_kind { INPUT_STATE, INPUT_SENSOR, INPUT_DISCONNECT };
struct input_event { enum input_kind kind; uint8_t peer, len, bytes[16]; uint32_t at; };
void dongle_input(const struct input_event *event);
void split_start(void);
void usb_start(void);
void usb_poll(void);
void usb_output(const struct output *out, void *context);
