#pragma once
#include <stdint.h>
#include <stdbool.h>
#define TUD_HID_REPORT_DESC_KEYBOARD(...) 0
#define TUD_HID_REPORT_DESC_CONSUMER(...) 0
#define TUD_HID_REPORT_DESC_MOUSE(...) 0
#define TUD_CONFIG_DESCRIPTOR(...) 0
#define TUD_HID_DESCRIPTOR(...) 0
#define CFG_TUD_HID_EP_BUFSIZE 64
typedef enum { HID_REPORT_TYPE_FEATURE=3 } hid_report_type_t;
bool tud_hid_ready(void);
bool tud_mounted(void);
bool tud_suspended(void);
bool tud_hid_keyboard_report(uint8_t id,uint8_t mods,const uint8_t keys[6]);
bool tud_hid_report(uint8_t id,const void *data,uint16_t len);
bool tud_hid_mouse_report(uint8_t id,uint8_t buttons,int8_t x,int8_t y,int8_t wheel,int8_t pan);
