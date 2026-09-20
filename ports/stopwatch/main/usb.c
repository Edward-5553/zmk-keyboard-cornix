// SPDX-License-Identifier: MIT
#include "dongle.h"
#include "display_status.h"
#include "keymap_config.h"
#include "keymap_protocol.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tusb.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const uint8_t report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(1)),
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(2)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(3)),
    // WebHID accesses only this vendor collection; standard input reports are protected.
    0x06,0x50,0xff,0x09,0x01,0xa1,0x01,0x85,KM_REPORT_ID,
    0x15,0x00,0x26,0xff,0x00,0x75,0x08,0x95,KM_REPORT_SIZE,
    0x09,0x01,0xb1,0x02,0xc0,
};
_Static_assert(CFG_TUD_HID_EP_BUFSIZE>=KM_REPORT_SIZE+1,"HID feature report buffer too small");
static const uint8_t config_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1,1,0,TUD_CONFIG_DESC_LEN+TUD_HID_DESC_LEN,0,100),
    TUD_HID_DESCRIPTOR(0,4,HID_ITF_PROTOCOL_NONE,sizeof(report_descriptor),0x81,16,1),
};
static char serial[13];
static struct output desired_keyboard;
static uint16_t desired_consumer;
static bool keyboard_dirty, consumer_dirty;
static const char *strings[] = {(char[]){9,4},"Cornix", "StopWatch Dongle (experimental)",serial,"Keyboard / media / wheel"};
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) { return report_descriptor; }
uint16_t tud_hid_get_report_cb(uint8_t instance,uint8_t report_id,hid_report_type_t type,uint8_t *buffer,uint16_t len) {
    return report_id==KM_REPORT_ID && type==HID_REPORT_TYPE_FEATURE?keymap_config_report(buffer,len):0;
}
void tud_hid_set_report_cb(uint8_t instance,uint8_t report_id,hid_report_type_t type,const uint8_t *buffer,uint16_t len) {
    if(report_id==KM_REPORT_ID && type==HID_REPORT_TYPE_FEATURE)keymap_config_receive(buffer,len);
}

void usb_start(void) {
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_read_mac(mac,ESP_MAC_WIFI_STA));
    snprintf(serial,sizeof(serial),"%02X%02X%02X%02X%02X%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    tinyusb_config_t cfg=TINYUSB_DEFAULT_CONFIG();
    cfg.descriptor.full_speed_config=config_descriptor;
    cfg.descriptor.string=strings;
    cfg.descriptor.string_count=5;
    ESP_ERROR_CHECK(tinyusb_driver_install(&cfg));
}

void usb_poll(void) {
    if (!tud_hid_ready()) return;
    if (keyboard_dirty) {
        if (tud_hid_keyboard_report(1,desired_keyboard.mods,desired_keyboard.keys)) {
            keyboard_dirty=false;display_status_keyboard(desired_keyboard.mods,desired_keyboard.keys);
        }
    } else if (consumer_dirty) {
        if (tud_hid_report(2,&desired_consumer,sizeof(desired_consumer))) consumer_dirty=false;
    }
}

void usb_output(const struct output *out, void *context) {
    if(out->kind==OUT_WHEEL && out->wheel)
        display_status_activity(out->wheel>0?DISPLAY_HINT_SCROLL_UP:DISPLAY_HINT_SCROLL_DOWN);
    if(out->kind==OUT_CONSUMER && (out->consumer==233 || out->consumer==234))
        display_status_activity(out->consumer==233?DISPLAY_HINT_VOL_UP:DISPLAY_HINT_VOL_DOWN);
    if (out->kind==OUT_WAIT) { vTaskDelay(pdMS_TO_TICKS(30)); return; }
    if (out->kind==OUT_KEYBOARD) { desired_keyboard=*out; keyboard_dirty=true; }
    if (out->kind==OUT_CONSUMER) { desired_consumer=out->consumer; consumer_dirty=true; }
    int64_t deadline=esp_timer_get_time()+100000;
    while (tud_mounted() && !tud_suspended() && esp_timer_get_time()<deadline) {
        usb_poll();
        if (!keyboard_dirty && !consumer_dirty && (out->kind!=OUT_WHEEL || tud_hid_ready())) break;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    // Keep keyboard/media releases pending across a busy or suspended endpoint.
    // Wheel reports are relative and can safely be dropped while disconnected.
    if (out->kind==OUT_WHEEL && tud_hid_ready()) tud_hid_mouse_report(3,0,0,0,out->wheel,0);
}
