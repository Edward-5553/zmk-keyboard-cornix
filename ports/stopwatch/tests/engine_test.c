// SPDX-License-Identifier: MIT
#include "engine.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct output log_entries[4096];
static unsigned count;
static void capture(const struct output *o, void *ctx) { assert(count<4096); log_entries[count++]=*o; }
static void reset(void) { engine_init(capture,NULL); count=0; }
static bool saw(unsigned from,uint8_t code,uint8_t mods) {
    for (unsigned i=from;i<count;i++) if (log_entries[i].kind==OUT_KEYBOARD && log_entries[i].mods==mods)
        for (unsigned j=0;j<6;j++) if (log_entries[i].keys[j]==code) return true;
    return false;
}
static struct output last_keyboard(void) {
    for (unsigned i=count;i>0;i--) if (log_entries[i-1].kind==OUT_KEYBOARD) return log_entries[i-1];
    assert(false); return (struct output){0};
}
static void up(void) {
    struct output o=last_keyboard();
    assert(o.mods==0);
    for (int i=0;i<6;i++) assert(o.keys[i]==0);
}
int main(void) {
    reset(); engine_position(1,true,0); assert(saw(0,20,0)); engine_position(1,false,1); up();
    reset(); engine_position(41,true,0); assert(saw(0,76,5)); engine_position(41,false,1); up();
    reset(); engine_position(43,true,0); engine_position(43,false,100); assert(saw(0,44,0)); up();
    // Holding Space enables Num/Nav; key release must use the original binding.
    reset(); engine_position(43,true,0); engine_tick(200); engine_position(3,true,201);
    assert(saw(0,82,0)); engine_position(43,false,202); engine_position(3,false,203); up();
    // Balanced: rolling press then release Space is a tap, not a layer hold.
    reset(); engine_position(43,true,0); engine_position(3,true,20); engine_position(43,false,50);
    assert(saw(0,44,0)); assert(saw(0,8,0)); assert(!saw(0,82,0)); engine_position(3,false,60); up();
    // Balanced: complete another tap while Space held resolves to Num/Nav.
    reset(); engine_position(43,true,0); engine_position(3,true,20); engine_position(3,false,40);
    assert(saw(0,82,0)); assert(!saw(0,44,0)); engine_position(43,false,50); up();
    // Quick second tap remains Space even after the hold timeout.
    reset(); engine_position(43,true,0); engine_position(43,false,30); unsigned begin=count;
    engine_position(43,true,80); engine_tick(400); engine_position(3,true,410);
    assert(saw(begin,44,0)); assert(saw(begin,8,0)); engine_position(3,false,420); engine_position(43,false,430); up();
    // Cancel does not turn a pending layer key into an unwanted Space.
    reset(); engine_position(43,true,0); engine_cancel(); assert(!saw(0,44,0)); up();
    // Macro produces { } Left, with Shift only on the bracket strokes.
    reset(); engine_position(44,true,0); engine_tick(201); engine_position(13,true,202);
    for(unsigned t=202;t<=382;t++)engine_tick(t);
    assert(saw(0,47,2)); assert(saw(0,48,2)); assert(saw(0,80,0));
    engine_position(13,false,210); engine_position(44,false,220); up();
    // Two physical Shift keys must keep Shift down until both are released.
    reset(); engine_position(24,true,0); engine_position(37,true,1); engine_position(24,false,2);
    assert(last_keyboard().mods==32); engine_position(37,false,3); up();
    // Layer bindings with transparent fallbacks keep base shortcuts.
    reset(); assert(engine_layer()==0); engine_position(43,true,0);
    assert(engine_layer()==0); engine_tick(201); assert(engine_layer()==1);
    engine_position(42,true,210);
    assert(saw(0,44,4)); engine_cancel(); assert(engine_layer()==0); up();
    reset(); for (int i=1;i<=7;i++) engine_position(i,true,i);
    struct output o=last_keyboard(); for (int i=0;i<6;i++) assert(o.keys[i]==1);
    engine_position(7,false,10); assert(last_keyboard().keys[0]!=1); engine_cancel(); up();
    // Millisecond wrap is safe.
    reset(); engine_position(43,true,UINT32_MAX-100); engine_tick(100);
    engine_position(3,true,101); assert(saw(0,82,0)); engine_cancel();
    // 18 degrees = one detent. Two 9 degree packets must preserve remainder.
    reset(); uint8_t sensor[14]={0,1,9}; assert(engine_sensor(sensor,14)); assert(count==0);
    assert(engine_sensor(sensor,14)); assert(count==1 && log_entries[0].wheel==-1);
    sensor[0]=1; sensor[2]=18; assert(engine_sensor(sensor,14));
    engine_tick(1);
    bool volume=false; for(unsigned i=0;i<count;i++) if(log_entries[i].consumer==233) volume=true;
    assert(volume); assert(!engine_sensor(sensor,13)); sensor[0]=2; assert(!engine_sensor(sensor,14));
    // Legacy negative tick and invalid giant tick.
    reset(); uint8_t legacy[14]={0,1,0,0,0,0,255,255,255,255};
    assert(engine_sensor(legacy,14)); assert(log_entries[0].wheel==1);
    legacy[6]=100; legacy[7]=legacy[8]=legacy[9]=0; assert(!engine_sensor(legacy,14));
    puts("StopWatch engine: 16 scenarios passed");
}
