// SPDX-License-Identifier: MIT
#include "display_status.h"
#include "freertos/FreeRTOS.h"
#include "esp_timer.h"
static portMUX_TYPE guard=portMUX_INITIALIZER_UNLOCKED;
static struct display_model state;
#define LOCK() portENTER_CRITICAL(&guard)
#define UNLOCK() portEXIT_CRITICAL(&guard)
void display_status_init(void) { display_model_init(&state); state.last_activity=esp_timer_get_time()/1000; }
void display_status_connect(unsigned p,const uint8_t id[7]) { LOCK(); display_model_connect(&state,p,id); UNLOCK(); }
void display_status_ready(unsigned p) { LOCK(); if(p<2)state.peers[p].ready=true; UNLOCK(); }
void display_status_disconnect(unsigned p) { LOCK(); display_model_disconnect(&state,p); UNLOCK(); }
void display_status_positions(unsigned p,const uint8_t bits[16]) { LOCK(); display_model_positions(&state,p,bits); UNLOCK(); }
void display_status_side(unsigned p,unsigned side) { LOCK(); display_model_side(&state,p,side); UNLOCK(); }
void display_status_battery(unsigned p,int level) { LOCK(); display_model_battery(&state,p,level); UNLOCK(); }
void display_status_pairing(uint32_t until) { LOCK(); state.pairing_until=until; UNLOCK(); }
void display_status_usb(enum display_usb usb) {
    uint32_t now=esp_timer_get_time()/1000;
    LOCK(); display_model_usb(&state,usb,now); UNLOCK();
}
void display_status_layer(uint8_t layer) { LOCK(); state.layer=layer; UNLOCK(); }
void display_status_ble_waiting(bool waiting) { LOCK();state.ble_waiting=waiting;UNLOCK(); }
void display_status_activity(enum display_hint hint) {
    uint32_t now=esp_timer_get_time()/1000;
    LOCK(); state.has_activity=true; state.last_activity=now;
    if(hint!=DISPLAY_HINT_NONE) {state.hint=hint;state.hint_at=now;}
    UNLOCK();
}
void display_status_snapshot(struct display_model *out) { LOCK(); *out=state; UNLOCK(); }

void display_status_keyboard(uint8_t mods,const uint8_t keys[6]) {
    uint32_t now=esp_timer_get_time()/1000;
    LOCK(); display_model_keyboard(&state,mods,keys,now); UNLOCK();
}
