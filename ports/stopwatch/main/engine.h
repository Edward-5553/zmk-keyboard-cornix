// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum binding_kind { B_NONE, B_TRANS, B_KEY, B_LT, B_MACRO };
struct binding { uint8_t kind, mods; uint16_t code; uint8_t page, layer; };
enum output_kind { OUT_KEYBOARD, OUT_CONSUMER, OUT_WHEEL, OUT_WAIT };
struct output { enum output_kind kind; uint8_t mods, keys[6]; uint16_t consumer; int8_t wheel; };
typedef void (*output_fn)(const struct output *, void *);
void engine_init(output_fn output, void *context);
void engine_position(uint8_t position, bool pressed, uint32_t now);
void engine_tick(uint32_t now);
uint8_t engine_layer(void);
void engine_cancel(void); // Disconnect/overflow: release everything, never emit a tap.
bool engine_sensor(const uint8_t *data, size_t size);

#define KEYMAP_LAYERS 5
#define KEYMAP_KEYS 50
#define KEYMAP_BYTES (KEYMAP_LAYERS * KEYMAP_KEYS * 6)
uint32_t engine_keymap_schema(void);
void engine_keymap_export(uint8_t out[KEYMAP_BYTES],bool defaults);
bool engine_keymap_validate(const uint8_t *data,size_t size);
bool engine_keymap_apply(const uint8_t *data,size_t size);
bool engine_idle(void);
