// SPDX-License-Identifier: MIT
#include "engine.h"
#include "zmk_compat.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct output events[2048];
static unsigned count;
static void capture(const struct output *o,void *ctx) {assert(count<2048);events[count++]=*o;}
static void reset(void) {engine_init(capture,NULL);count=0;}
static bool contains(struct output o,unsigned key) {
    for(unsigned i=0;i<6;i++)if(o.keys[i]==key)return true;
    return false;
}
static struct output keyboard(void) {
    for(unsigned i=count;i>0;i--)if(events[i-1].kind==OUT_KEYBOARD)return events[i-1];
    return (struct output){0};
}
static unsigned consumer(void) {
    for(unsigned i=count;i>0;i--)if(events[i-1].kind==OUT_CONSUMER)return events[i-1].consumer;
    return 0;
}
int main(void) {
    assert(SPLIT_INTERVAL==6 && SPLIT_LATENCY==30 && SPLIT_TIMEOUT==400);
    // A plain key emits only its page; no redundant consumer report per edge.
    reset();engine_position(1,true,1);engine_position(1,false,2);
    assert(count==2 && events[0].kind==OUT_KEYBOARD && contains(events[0],20));
    // Layer-tap down/up and the next letter need no time advance or artificial wait.
    reset();engine_position(43,true,0);engine_position(43,false,50);
    assert(count==2 && contains(events[0],44) && !contains(events[1],44));
    engine_position(1,true,51);assert(contains(keyboard(),20));
    // ZMK replays captured positions between the tap press and tap release.
    reset();engine_position(43,true,0);engine_position(3,true,20);engine_position(43,false,50);
    assert(count==3 && contains(events[0],44) && contains(events[1],44) &&
           contains(events[1],8) && !contains(events[2],44) && contains(events[2],8));
    // Press-to-press, not release-to-press: 160 ms must allow a hold.
    reset();engine_position(43,true,0);engine_position(43,false,140);
    engine_position(43,true,160);engine_tick(360);assert(engine_layer()==1);
    // A different non-modifier in between invalidates quick-tap.
    reset();engine_position(43,true,0);engine_position(43,false,20);
    engine_position(1,true,30);engine_position(1,false,40);
    engine_position(43,true,50);engine_tick(250);assert(engine_layer()==1);
    // A true quick-tap uses the regular held-key path, including through a transparent layer.
    reset();engine_position(43,true,0);engine_position(43,false,20);
    engine_position(43,true,100);engine_tick(350);assert(engine_layer()==0 && contains(keyboard(),44));
    // Existing ordinary keys can release while a new hold-tap remains undecided.
    reset();engine_position(1,true,0);engine_position(43,true,10);engine_position(1,false,20);
    assert(!contains(keyboard(),20) && engine_layer()==0);
    engine_position(43,false,30);assert(engine_idle());
    // Scheduling work cannot turn a queued 190 ms tap into a hold at wall time 205.
    reset();engine_position(43,true,0);engine_poll(205);engine_position(43,false,190);
    assert(count==2 && contains(events[0],44) && engine_layer()==0);
    // Macro timing: press 30 ms, release/wait 30 ms, while real keys remain responsive.
    reset();engine_position(44,true,0);engine_tick(200);engine_position(13,true,201);
    engine_tick(201);assert(contains(keyboard(),47) && keyboard().mods==2);
    // Position 40 is unassigned here; release the layer to type a base Q during the macro.
    engine_position(44,false,202);engine_position(1,true,203);
    assert(contains(keyboard(),20) && contains(keyboard(),47));
    engine_position(1,false,204);assert(!contains(keyboard(),20) && contains(keyboard(),47));
    engine_tick(230);assert(contains(keyboard(),47));
    engine_tick(231);assert(!contains(keyboard(),47));
    engine_tick(260);assert(!contains(keyboard(),48));
    engine_tick(261);assert(contains(keyboard(),48));
    engine_tick(291);engine_tick(321);assert(contains(keyboard(),80));
    engine_tick(351);assert(!contains(keyboard(),80));engine_tick(381);
    engine_position(13,false,382);assert(engine_idle());
    // Volume uses ZMK's 5 ms sensor tap, no additional 30 ms gap.
    reset();uint8_t sensor[14]={1,1,36};assert(engine_sensor(sensor,14));
    engine_tick(100);assert(consumer()==233);
    engine_position(1,true,101);assert(contains(keyboard(),20));
    engine_tick(104);assert(consumer()==233);
    unsigned before=count;engine_tick(105);
    assert(count==before+2 && events[before].consumer==0 && events[before+1].consumer==233);
    engine_tick(110);assert(consumer()==0);
    // Disconnect cancels delayed work; it must not emit stale macro or volume events later.
    reset();assert(engine_sensor(sensor,14));engine_tick(0);engine_cancel();
    before=count;engine_tick(1000);assert(count==before && engine_idle() && !consumer());
    // Deadline wrap and a late scheduler still preserve the minimum press duration.
    reset();sensor[2]=18;assert(engine_sensor(sensor,14));engine_tick(UINT32_MAX-2);
    engine_tick(1);assert(consumer()==233);engine_tick(2);assert(consumer()==0);
    reset();sensor[2]=36;assert(engine_sensor(sensor,14));engine_tick(0);
    engine_tick(100);assert(consumer()==233);engine_tick(104);assert(consumer()==233);
    engine_tick(105);assert(consumer()==0);
    // Bounded behavior queue fails by releasing, never leaving a synthetic key down.
    reset();sensor[2]=18;
    for(unsigned i=0;i<64;i++)assert(engine_sensor(sensor,14));
    assert(!engine_sensor(sensor,14));assert(engine_idle());
    before=count;engine_tick(1000);assert(count==before);
    puts("ZMK timing: tap/replay, quick-tap, macro/typing, volume, cancel, wrap and overflow passed");
}
