// SPDX-License-Identifier: MIT
#include "engine.h"
#include "keymap_generated.h"
#include "zmk_compat.h"
#include <string.h>

static output_fn emit;
static void *emit_context;
static struct binding active[50];
static bool down[50];
static uint16_t layers[LAYER_COUNT];
static int pending;
static uint32_t pending_at, last_tap_at;
static int last_tap_position;
struct edge { uint8_t pos; bool down; uint32_t at; };
static struct edge deferred[128];
static unsigned deferred_count;
static int64_t sensor_remainder[2];
static struct binding live_keymap[LAYER_COUNT][50];
// ZMK's behavior queue delays synthetic behaviors, never the physical input task.
#define TAP_QUEUE_SIZE 64
struct queued_tap { struct binding binding; uint16_t tap_ms, wait_ms; };
static struct queued_tap tap_queue[TAP_QUEUE_SIZE];
static unsigned tap_head,tap_count;
static unsigned tap_phase; // 0=start, 1=pressed, 2=inter-behavior wait
static uint32_t tap_deadline;
static struct binding synthetic;
static struct output last_keyboard,last_consumer;
static bool reports_valid;
_Static_assert(LAYER_COUNT==KEYMAP_LAYERS,"Update runtime protocol for layer count changes");

uint32_t engine_keymap_schema(void) {return KEYMAP_SCHEMA;}
bool engine_idle(void) {
    if(tap_count)return false;
    for(unsigned i=0;i<50;i++)if(down[i])return false;
    return true;
}
void engine_keymap_export(uint8_t out[KEYMAP_BYTES],bool defaults) {
    for(unsigned l=0;l<LAYER_COUNT;l++)for(unsigned p=0;p<50;p++) {
        struct binding b=defaults?keymap[l][p]:live_keymap[l][p];
        unsigned o=(l*50+p)*6;
        out[o]=b.kind;out[o+1]=b.mods;out[o+2]=b.code;out[o+3]=b.code>>8;out[o+4]=b.page;out[o+5]=b.layer;
    }
}
static struct binding decode_binding(const uint8_t *p) {
    return (struct binding){.kind=p[0],.mods=p[1],.code=p[2]|(uint16_t)p[3]<<8,.page=p[4],.layer=p[5]};
}
bool engine_keymap_validate(const uint8_t *data,size_t size) {
    if(!data || size!=KEYMAP_BYTES)return false;
    for(unsigned i=0;i<size;i+=6) {
        struct binding b=decode_binding(data+i);
        if(b.kind==B_NONE || b.kind==B_TRANS) {
            if(b.mods || b.code || b.page || b.layer)return false;
        } else if(b.kind==B_KEY || b.kind==B_LT) {
            if(b.kind==B_KEY && b.layer)return false;
            if(b.kind==B_LT && (!b.layer || b.layer>=LAYER_COUNT || b.page!=7))return false;
            if(b.page==7) {if(b.code<4 || b.code>231)return false;}
            else if(b.page==12) {if(!b.code || b.code>0x3ff || b.mods)return false;}
            else return false;
        } else if(b.kind==B_MACRO) {
            if(b.code>=sizeof(macro_lengths)/sizeof(macro_lengths[0]) || b.mods || b.page || b.layer)return false;
        } else return false;
    }
    return true;
}
bool engine_keymap_apply(const uint8_t *data,size_t size) {
    if(!engine_keymap_validate(data,size))return false;
    engine_cancel();
    for(unsigned l=0;l<LAYER_COUNT;l++)for(unsigned p=0;p<50;p++)
        live_keymap[l][p]=decode_binding(data+(l*50+p)*6);
    return true;
}

uint8_t engine_layer(void) {
    for (int l=LAYER_COUNT-1;l>0;l--) if(layers[l])return l;
    return 0;
}

static void report(struct binding extra) {
    struct output keyboard = {.kind=OUT_KEYBOARD};
    struct output consumer = {.kind=OUT_CONSUMER};
    bool keys[256] = {0};
    for (int i=0; i<=51; i++) {
        struct binding b = i==51 ? synthetic : i==50 ? extra : active[i];
        if (b.kind != B_KEY) continue;
        keyboard.mods |= b.mods;
        if (b.page==12) consumer.consumer=b.code;
        else if (b.page==7 && b.code>=224 && b.code<=231) keyboard.mods |= 1u<<(b.code-224);
        else if (b.page==7 && b.code<256) keys[b.code]=true;
    }
    unsigned count=0;
    for (unsigned k=1; k<256; k++) if (keys[k]) {
        if (count<6) keyboard.keys[count]=k;
        count++;
    }
    if (count>6) memset(keyboard.keys, 1, sizeof(keyboard.keys)); // HID ErrorRollOver
    if(!reports_valid || keyboard.mods!=last_keyboard.mods ||
       memcmp(keyboard.keys,last_keyboard.keys,sizeof(keyboard.keys)))emit(&keyboard,emit_context);
    if(!reports_valid || consumer.consumer!=last_consumer.consumer)emit(&consumer,emit_context);
    last_keyboard=keyboard;last_consumer=consumer;reports_valid=true;
}

static bool queue_taps(const struct binding *bindings,unsigned count,unsigned tap_ms,unsigned wait_ms) {
    if(count>TAP_QUEUE_SIZE-tap_count) {engine_cancel();return false;}
    for(unsigned i=0;i<count;i++) {
        unsigned slot=(tap_head+tap_count++)%TAP_QUEUE_SIZE;
        tap_queue[slot]=(struct queued_tap){bindings[i],tap_ms,wait_ms};
        tap_queue[slot].binding.kind=B_KEY;
    }
    return true;
}

static void tick_taps(uint32_t now) {
    while(tap_count) {
        struct queued_tap *t=&tap_queue[tap_head];
        if(tap_phase && (int32_t)(now-tap_deadline)<0)return;
        if(tap_phase==0) {
            last_tap_position=-1;last_tap_at=now;
            synthetic=t->binding;report((struct binding){0});
            tap_phase=1;tap_deadline=now+t->tap_ms;
            if(t->tap_ms)return;
        } else if(tap_phase==1) {
            synthetic=(struct binding){0};report((struct binding){0});
            tap_phase=2;tap_deadline=now+t->wait_ms;
            if(t->wait_ms)return;
        } else {
            tap_head=(tap_head+1)%TAP_QUEUE_SIZE;tap_count--;tap_phase=0;
        }
    }
}

static struct binding resolve(unsigned pos) {
    for (int l=LAYER_COUNT-1; l>=0; l--) {
        if (l && !layers[l]) continue;
        if (live_keymap[l][pos].kind!=B_TRANS) return live_keymap[l][pos];
    }
    return (struct binding){0};
}

static void replay(void) {
    struct edge copy[128];
    unsigned count=deferred_count;
    memcpy(copy,deferred,count*sizeof(*copy));
    deferred_count=0;
    for (unsigned i=0; i<count; i++) engine_position(copy[i].pos,copy[i].down,copy[i].at);
}

static void hold_pending(void) {
    int pos=pending;
    pending=-1;
    layers[active[pos].layer]++;
    replay();
}

void engine_init(output_fn output, void *context) {
    memcpy(live_keymap,keymap,sizeof(live_keymap));
    emit=output;
    emit_context=context;
    engine_cancel();
}

void engine_cancel(void) {
    memset(active,0,sizeof(active));
    memset(down,0,sizeof(down));
    memset(layers,0,sizeof(layers));
    last_tap_position=-1;last_tap_at=0;
    tap_head=tap_count=tap_phase=0;
    synthetic=(struct binding){0};
    memset(sensor_remainder,0,sizeof(sensor_remainder));
    pending=-1;
    deferred_count=0;
    reports_valid=false;
    if (emit) {
        emit(&(struct output){.kind=OUT_RESET},emit_context);
        report((struct binding){0});
    }
}

void engine_tick(uint32_t now) {
    // Unsigned subtraction handles the millisecond counter wrapping.
    if (pending>=0 && (uint32_t)(now-pending_at)>=HOLD_TAP_TERM_MS) hold_pending();
    tick_taps(now);
}

void engine_poll(uint32_t now) {tick_taps(now);}

void engine_position(uint8_t pos, bool pressed, uint32_t now) {
    if (pos>=50) return;
    // Replayed position timestamps must not advance the synthetic behavior clock.
    if(pending>=0 && (uint32_t)(now-pending_at)>=HOLD_TAP_TERM_MS)hold_pending();
    if (pending>=0) {
        if (pos==pending && !pressed) {
            active[pos].kind=B_KEY;
            down[pos]=false;
            pending=-1;
            last_tap_position=pos;
            last_tap_at=pending_at; // ZMK quick-tap measures press-to-press.
            report((struct binding){0});
            replay();
            active[pos]=(struct binding){0};
            report((struct binding){0});
            return;
        }
        if (pos==pending) return;
        // Balanced flavor: decide hold on an intervening key's full press/release.
        bool intervening=false;
        for (unsigned i=0; i<deferred_count; i++)
            if (deferred[i].pos==pos && deferred[i].down) intervening=true;
        bool modifier=active[pos].kind==B_KEY && active[pos].page==7 &&
                      active[pos].code>=224 && active[pos].code<=231;
        // ZMK lets releases of previously held non-modifier keys pass through.
        if(pressed || intervening || modifier) {
            if (deferred_count==128) { engine_cancel(); return; }
            deferred[deferred_count++]=(struct edge){pos,pressed,now};
            if (!pressed && intervening) hold_pending();
            return;
        }
    }
    if (down[pos]==pressed) return;
    down[pos]=pressed;
    if (!pressed) {
        struct binding b=active[pos];
        active[pos]=(struct binding){0};
        if (b.kind==B_LT && layers[b.layer]) layers[b.layer]--;
        report((struct binding){0});
        return;
    }
    struct binding b=resolve(pos);
    bool layer_tap=b.kind==B_LT;
    if (b.kind==B_LT) {
        if (last_tap_position==pos && (uint32_t)(now-last_tap_at)<QUICK_TAP_MS) {
            b.kind=B_KEY; // Quick second tap can be held for host key repeat.
            last_tap_at=now;
        } else {
            active[pos]=b;
            pending=pos;
            pending_at=now;
            return;
        }
    }
    active[pos]=b;
    if(b.kind==B_KEY && !(b.page==7 && b.code>=224 && b.code<=231) &&
       !layer_tap && (int32_t)(now-last_tap_at)>0) {
        last_tap_position=-1;last_tap_at=now;
    }
    if (b.kind==B_MACRO) {
        queue_taps(macros[b.code],macro_lengths[b.code],MACRO_TAP_MS,MACRO_WAIT_MS);
    } else report((struct binding){0});
}

static int32_t le32(const uint8_t *p) {
    return (int32_t)((uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24);
}

bool engine_sensor(const uint8_t *data, size_t size) {
    // ZMK sensor_event: index, channel count, packed int32 val1/val2/channel.
    if (size!=14 || data[0]>1 || data[1]!=1) return false;
    int32_t val1=le32(data+2), val2=le32(data+6);
    int64_t ticks;
    if (val1==0) ticks=val2; // Legacy ZMK EC11 packet.
    else {
        sensor_remainder[data[0]]+=(int64_t)val1*1000000+val2;
        ticks=sensor_remainder[data[0]]/18000000; // 20 triggers per revolution.
        sensor_remainder[data[0]]%=18000000;
    }
    if (ticks>32 || ticks< -32) { sensor_remainder[data[0]]=0; return false; }
    int direction=ticks>0 ? 1 : -1;
    for (int64_t i=0; i<(ticks>0?ticks:-ticks); i++) {
        if (data[0]==0) {
            struct output out={.kind=OUT_WHEEL,.wheel=-direction};
            emit(&out,emit_context);
        } else {
            struct binding b={.kind=B_KEY,.code=direction>0?233:234,.page=12};
            if(!queue_taps(&b,1,SENSOR_TAP_MS,0))return false;
        }
    }
    return true;
}
