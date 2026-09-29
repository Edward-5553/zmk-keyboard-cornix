// SPDX-License-Identifier: MIT
#include "output_route.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct {struct output o;unsigned host;uint32_t session;} events[128];
static unsigned count;
static void usb(const struct output *o,void *ctx) {
    assert(count<128);events[count].o=*o;events[count++].host=1;
}
static void ble(const struct output *o,void *ctx) {
    assert(count<128);events[count].o=*o;events[count].session=(uintptr_t)ctx;events[count++].host=2;
}
static void released(unsigned host) {
    bool keyboard=false,consumer=false;
    for(unsigned i=0;i<count;i++)if(events[i].host==host){
        if(events[i].o.kind==OUT_KEYBOARD){
            assert(!events[i].o.mods);
            for(unsigned k=0;k<6;k++)assert(!events[i].o.keys[k]);
            keyboard=true;
        }
        if(events[i].o.kind==OUT_CONSUMER){assert(!events[i].o.consumer);consumer=true;}
    }
    assert(keyboard && consumer);
}
int main(void) {
    assert(output_route_select(false,false,0)==OUTPUT_OFF);
    assert(output_route_select(false,false,1)==OUTPUT_BLE);
    assert(output_route_select(true,false,1)==OUTPUT_USB);
    assert(output_route_select(true,true,1)==OUTPUT_OFF);
    struct output_router r={.usb=usb,.ble=ble};
    engine_init(output_router_send,&r);
    assert(output_router_update(&r,false,false,1,false));
    count=0;engine_position(1,true,0);engine_position(1,false,1);
    assert(count>=2 && events[0].host==2 && events[0].o.keys[0]==20);
    assert(events[0].session==1);
    // Switching while Ctrl+Alt+Delete is held must release both destinations.
    engine_position(41,true,2);count=0;
    assert(output_router_update(&r,true,false,1,false));
    released(1);released(2);assert(engine_idle());
    assert(events[0].host==2 && events[count-1].host==1);
    // Repeated polls do not send releases or cancel a held layer.
    engine_position(43,true,10);count=0;
    assert(!output_router_update(&r,true,false,1,false));assert(!count);
    // Pending layer tap is cancelled rather than typed when the USB computer sleeps.
    assert(output_router_update(&r,true,true,1,false));released(1);
    for(unsigned i=0;i<count;i++)assert(events[i].host!=2);
    assert(engine_idle());
    assert(output_router_update(&r,false,false,1,false));
    engine_position(43,true,100);engine_tick(301);assert(engine_layer()==1);
    count=0;assert(output_router_update(&r,false,false,2,false));
    released(2);assert(engine_layer()==0 && engine_idle());
    assert(events[count-1].session==2); // Reconnect is detected even if READY was never polled false.
    count=0;assert(output_router_update(&r,false,false,2,true));released(2);
    count=0;assert(output_router_update(&r,false,false,0,false));released(2);
    count=0;engine_position(1,true,500);assert(!count);
    uint8_t bytes[8];unsigned index=99;
    struct output o={.kind=OUT_KEYBOARD,.mods=5,.keys={76,4,5,6,7,8}};
    const uint8_t expected[]={5,0,76,4,5,6,7,8};
    assert(output_hid_report(&o,bytes,&index)==8 && index==0 && !memcmp(bytes,expected,8));
    o=(struct output){.kind=OUT_CONSUMER,.consumer=0x1234};
    assert(output_hid_report(&o,bytes,&index)==2 && index==1 && bytes[0]==0x34 && bytes[1]==0x12);
    o=(struct output){.kind=OUT_WHEEL,.wheel=-1};
    assert(output_hid_report(&o,bytes,&index)==5 && index==2 && bytes[3]==255);
    assert(!bytes[0] && !bytes[1] && !bytes[2] && !bytes[4]);
    o.kind=OUT_RESET;assert(!output_hid_report(&o,bytes,&index));
    puts("USB/BLE routing, ordered tap, release, suspend and reconnect tests passed");
}
