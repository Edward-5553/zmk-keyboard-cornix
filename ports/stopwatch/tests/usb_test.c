// SPDX-License-Identifier: MIT
// Compile the actual usb.c with a controllable endpoint, not a mirror of the FIFO.
#include "dongle.h"
#include "display_status.h"
#include "tinyusb.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int64_t now;
static bool ready,accept=true,mounted=true,suspended;
static struct output received[2048];
static unsigned count;
int64_t esp_timer_get_time(void) {return now;}
bool tud_hid_ready(void) {return ready && mounted && !suspended;}
bool tud_mounted(void) {return mounted;}
bool tud_suspended(void) {return suspended;}
static bool sent(struct output o) {
    if(!accept)return false;
    assert(ready && count<2048);received[count++]=o;ready=false;return true;
}
bool tud_hid_keyboard_report(uint8_t id,uint8_t mods,const uint8_t keys[6]) {
    struct output o={.kind=OUT_KEYBOARD,.mods=mods};memcpy(o.keys,keys,6);return sent(o);
}
bool tud_hid_report(uint8_t id,const void *data,uint16_t len) {
    uint16_t value;assert(len==2);memcpy(&value,data,2);return sent((struct output){.kind=OUT_CONSUMER,.consumer=value});
}
bool tud_hid_mouse_report(uint8_t id,uint8_t buttons,int8_t x,int8_t y,int8_t wheel,int8_t pan) {
    return sent((struct output){.kind=OUT_WHEEL,.wheel=wheel});
}
int tinyusb_driver_install(const tinyusb_config_t *cfg) {return 0;}
int esp_read_mac(uint8_t *mac,int type) {memset(mac,0,6);return 0;}
void ble_output_status(uint8_t *out) {memset(out,0,8);}
void ble_output_forget(void) {}
size_t keymap_config_report(uint8_t *out,size_t len) {return 0;}
void keymap_config_receive(const uint8_t *data,size_t len) {}
void display_status_activity(enum display_hint hint) {}
void display_status_keyboard(uint8_t mods,const uint8_t keys[6]) {}
static void drain(void) {for(unsigned i=0;i<128;i++){ready=true;usb_poll();}ready=false;}
static void reset(void) {
    now=0;ready=false;accept=true;mounted=true;suspended=false;
    engine_init(usb_output,NULL);drain();count=0;
}
int main(void) {
    reset();
    // Immediate hold-tap edges survive a busy endpoint and precede the next letter.
    engine_position(43,true,0);engine_position(43,false,10);
    engine_position(1,true,11);engine_position(1,false,12);assert(count==0);
    drain();assert(count==4);
    assert(received[0].keys[0]==44 && received[1].keys[0]==0 &&
           received[2].keys[0]==20 && received[3].keys[0]==0);
    // A rejected submission remains at the head; keyboard/media/wheel order is stable.
    reset();struct output o={.kind=OUT_KEYBOARD,.keys={4}};
    ready=true;accept=false;usb_output(&o,NULL);assert(count==0);
    o=(struct output){.kind=OUT_CONSUMER,.consumer=233};usb_output(&o,NULL);
    o=(struct output){.kind=OUT_WHEEL,.wheel=-1};usb_output(&o,NULL);
    accept=true;drain();assert(count==3 && received[0].keys[0]==4 &&
                             received[1].consumer==233 && received[2].wheel==-1);
    // Cancel removes unsent input before releasing the host, including after suspend.
    reset();engine_position(1,true,1);suspended=true;engine_cancel();drain();assert(count==0);
    suspended=false;drain();
    assert(count==2 && received[0].keys[0]==0 && received[1].consumer==0);
    // FIFO capacity never overwrites an earlier press with a later release.
    reset();o=(struct output){.kind=OUT_KEYBOARD,.keys={4}};
    for(unsigned i=0;i<65;i++)usb_output(&o,NULL);
    assert(usb_take_overflow());engine_cancel();drain();assert(count==2 && !received[0].keys[0]);
    // Stall detection is nonblocking and asks the main task for release/reset recovery.
    reset();usb_output(&o,NULL);now=99999;usb_poll();assert(!usb_take_overflow());
    now=100000;usb_poll();assert(usb_take_overflow());engine_cancel();drain();
    assert(count==2 && !received[0].keys[0]);
    // Suspension alone is not reported as a timed-out live endpoint.
    reset();usb_output(&o,NULL);suspended=true;now=1000000;usb_poll();assert(!usb_take_overflow());
    puts("USB actual transport: busy/retry FIFO, fast tap, cancel, overflow and stall recovery passed");
}
