// SPDX-License-Identifier: MIT
#include "output_route.h"
#include <string.h>
enum output_route output_route_select(bool mounted,bool suspended,uint32_t ble_session) {
    // Do not type into another host when the USB computer merely goes to sleep.
    if(mounted)return suspended?OUTPUT_OFF:OUTPUT_USB;
    return ble_session?OUTPUT_BLE:OUTPUT_OFF;
}
void output_router_send(const struct output *out,void *context) {
    struct output_router *r=context;
    if(r->route==OUTPUT_USB)r->usb(out,NULL);
    else if(r->route==OUTPUT_BLE)r->ble(out,(void *)(uintptr_t)r->session);
}
bool output_router_update(struct output_router *r,bool mounted,bool suspended,uint32_t session,bool overflow) {
    enum output_route next=output_route_select(mounted,suspended,session);
    if(next==r->route && (next!=OUTPUT_BLE || session==r->session) && !overflow)return false;
    engine_cancel(); // Releases go to the old host; cancel never produces a layer-key tap.
    r->route=next;r->session=session;
    engine_cancel(); // Clear any state retained by the new host before accepting input.
    return true;
}
unsigned output_hid_report(const struct output *o,uint8_t bytes[8],unsigned *index) {
    memset(bytes,0,8);
    if(o->kind==OUT_KEYBOARD){*index=0;bytes[0]=o->mods;memcpy(bytes+2,o->keys,6);return 8;}
    if(o->kind==OUT_CONSUMER){*index=1;bytes[0]=o->consumer;bytes[1]=o->consumer>>8;return 2;}
    if(o->kind==OUT_WHEEL){*index=2;bytes[3]=(uint8_t)o->wheel;return 5;}
    return 0;
}
