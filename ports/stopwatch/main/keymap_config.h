// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
void keymap_config_init(void);
void keymap_config_receive(const uint8_t *data,size_t size);
size_t keymap_config_report(uint8_t *out,size_t size);
bool keymap_config_poll(void);
void keymap_config_disconnect(void);
