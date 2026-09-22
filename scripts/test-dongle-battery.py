"""Check battery/modifier display filtering with real widgets and host stubs."""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
STUB = r'''
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#define IS_ENABLED(x) (x)
#define CONFIG_USB_DEVICE_STACK 1
#define ZMK_SPLIT_BLE_PERIPHERAL_COUNT 2
#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
#define LOG_MODULE_DECLARE(...)
#define LOG_DBG(...)
typedef struct node { struct node *next; } sys_snode_t;
typedef struct { sys_snode_t *head; } sys_slist_t;
#define SYS_SLIST_STATIC_INIT(x) {0}
static void sys_slist_append(sys_slist_t *s,sys_snode_t *n){n->next=s->head;s->head=n;}
#define SYS_SLIST_FOR_EACH_CONTAINER(s,w,m) for(sys_snode_t *n=(s)->head;n && ((w)=(void *)((char *)n - offsetof(__typeof__(*(w)),m)),1);n=n->next)
typedef int lv_color_t;
typedef struct {char text[32]; bool hidden;} lv_obj_t;
typedef int lv_layer_t;
typedef struct {int bg_color,bg_opa,border_color,border_width;} lv_draw_rect_dsc_t;
typedef struct {int x1,y1,x2,y2;} lv_area_t;
#define LV_CANVAS_BUF_SIZE(...) 40
#define LV_OPA_COVER 255
#define LV_OPA_TRANSP 0
#define LV_OBJ_FLAG_HIDDEN 1
#define LV_SIZE_CONTENT 0
#define LV_COLOR_FORMAT_L8 0
#define LV_ALIGN_TOP_RIGHT 0
#define LV_ALIGN_OUT_LEFT_MID 0
#define lv_color_black() 0
#define lv_color_white() 1
static unsigned draw_count, label_count;
static bool event_context, mutex_locked;
static void draw_record(void){assert(!event_context && !mutex_locked);draw_count++;}
#define lv_canvas_fill_bg(...) draw_record()
#define lv_canvas_init_layer(...) ((void)0)
#define lv_draw_rect_dsc_init(...) ((void)0)
#define lv_canvas_set_px(...) ((void)0)
#define lv_draw_rect(...) ((void)0)
#define lv_canvas_finish_layer(...) ((void)0)
#define lv_obj_move_foreground(...) ((void)0)
#define lv_obj_align_to(...) ((void)0)
#define lv_obj_align(...) ((void)0)
#define lv_obj_set_size(...) ((void)0)
#define lv_canvas_set_buffer(...) ((void)0)
static lv_obj_t objects[10];static int object_count;
static lv_obj_t *lv_obj_create(lv_obj_t *p){return &objects[object_count++];}
#define lv_canvas_create lv_obj_create
#define lv_label_create lv_obj_create
static void lv_obj_clear_flag(lv_obj_t *o,int f){o->hidden=false;}
static void lv_obj_add_flag(lv_obj_t *o,int f){o->hidden=true;}
static void lv_label_set_text_fmt(lv_obj_t *o,const char *f,...){assert(!event_context);label_count++;va_list a;va_start(a,f);vsnprintf(o->text,sizeof(o->text),f,a);va_end(a);}
struct zmk_position_state_changed {uint8_t source;uint32_t position;bool state;};
struct zmk_peripheral_battery_state_changed {uint8_t source,state_of_charge;};
struct zmk_battery_state_changed {uint8_t state_of_charge;};
typedef struct {int type;union {struct zmk_position_state_changed pos;struct zmk_peripheral_battery_state_changed bat;struct zmk_battery_state_changed own;};} zmk_event_t;
static const struct zmk_position_state_changed *as_zmk_position_state_changed(const zmk_event_t *e){return e->type==1?&e->pos:NULL;}
static const struct zmk_peripheral_battery_state_changed *as_zmk_peripheral_battery_state_changed(const zmk_event_t *e){return e->type==2?&e->bat:NULL;}
static const struct zmk_battery_state_changed *as_zmk_battery_state_changed(const zmk_event_t *e){return e->type==3?&e->own:NULL;}
static int zmk_battery_state_of_charge(void){return 90;}
static bool usb_powered=true;
static bool zmk_usb_is_powered(void){return usb_powered;}
struct zmk_key_physical_attrs {int16_t x;};
struct zmk_physical_layout {const struct zmk_key_physical_attrs *keys;size_t keys_len;};
static const struct zmk_key_physical_attrs keys[]={{0},{600},{750},{1350},{700}};
static const struct zmk_physical_layout layout={keys,5};
static const struct zmk_physical_layout *layouts[]={&layout};
static int selected;
static size_t zmk_physical_layouts_get_list(const struct zmk_physical_layout *const **p){*p=layouts;return 1;}
static int zmk_physical_layouts_get_selected(void){return selected;}
#define K_FOREVER 0
#define K_MUTEX_DEFINE(name) static int name
static void k_mutex_lock(int *m,int timeout){assert(!mutex_locked);mutex_locked=true;}
static void k_mutex_unlock(int *m){assert(mutex_locked);mutex_locked=false;}
struct k_work {void (*handler)(struct k_work *);};
#define K_WORK_DEFINE(name,fn) static struct k_work name={fn}
static struct k_work *pending;
static unsigned submissions;
static void *zmk_display_work_q(void){return NULL;}
static int k_work_submit_to_queue(void *q,struct k_work *w){assert(!mutex_locked);assert(!pending || pending==w);pending=w;submissions++;return 0;}
static bool display_initialized;
static bool zmk_display_is_initialized(void){return display_initialized;}
static void flush(void){assert(!event_context);if(pending){struct k_work *w=pending;pending=NULL;w->handler(w);}}
#define ZMK_EV_EVENT_BUBBLE 0
#define ZMK_LISTENER(name,cb) static int (*name##_listener)(const zmk_event_t *)=cb
#define ZMK_SUBSCRIPTION(...)
'''
TEST = r'''
#include "battery_status.c"
static void send(zmk_event_t e){event_context=true;assert(widget_dongle_battery_status_listener(&e)==ZMK_EV_EVENT_BUBBLE);event_context=false;}
static void battery(int source,int level){send((zmk_event_t){.type=2,.bat={source,level}});}
static void key(int source,int pos,bool down){send((zmk_event_t){.type=1,.pos={source,pos,down}});}
int main(int argc,char **argv){
  int left=argc>1?1:0,right=1-left;
  battery(left,10);key(left,0,true);assert(!pending && submissions==0);
  struct zmk_widget_dongle_battery_status widget={0};
  zmk_widget_dongle_battery_status_init(&widget,NULL);
  display_initialized=true;
  assert(battery_objects[SOURCE_OFFSET].label->hidden);
  unsigned draws=draw_count;
  battery(left,81);
  battery(right,42);
  assert(draw_count==draws); /* All rendering stays on the display queue. */
  flush();assert(draw_count==draws+2);
  assert(!strcmp(battery_objects[left+SOURCE_OFFSET].label->text,"? 81% "));
  assert(!strcmp(battery_objects[right+SOURCE_OFFSET].label->text,"? 42% "));
  unsigned submitted=submissions;
  key(left,0,false); key(255,0,true);key(left,99,true);key(left,4,true);
  struct battery_snapshot s=widget_dongle_battery_status_get_local_state();
  assert(!s.sources[left+SOURCE_OFFSET].side && submissions==submitted);
  key(left,1,true);key(right,2,true);flush();
  assert(!strcmp(battery_objects[left+SOURCE_OFFSET].label->text,"L 81% "));
  assert(!strcmp(battery_objects[right+SOURCE_OFFSET].label->text,"R 42% "));
  draws=draw_count;submitted=submissions;unsigned labels=label_count;
  for(int i=0;i<1000;i++){key(left,0,true);key(left,0,false);key(right,3,true);key(right,3,false);}
  battery(left,81);battery(right,42);flush();
  assert(submissions==submitted && draw_count==draws && label_count==labels);
  battery_status_update_cb(widget_dongle_battery_status_get_local_state());
  assert(draw_count==draws); /* A repeated work callback also avoids redraws. */
  battery(left,100);flush();s=widget_dongle_battery_status_get_local_state();
  assert(draw_count==draws+1); /* Only the changed row is drawn. */
  assert(!strcmp(battery_objects[left+SOURCE_OFFSET].label->text,"L100% "));
  assert(s.sources[right+SOURCE_OFFSET].level==42);
  submitted=submissions;battery(9,20);battery(255,20);
  s=widget_dongle_battery_status_get_local_state();
  assert(submissions==submitted && s.sources[left+SOURCE_OFFSET].level==100);
  assert(s.sources[0].level==(SOURCE_OFFSET?90:(left==0?100:42)));
  /* A -> B -> A before rendering must retain A, not leave B pending. */
  draws=draw_count;battery(left,20);battery(right,43);battery(left,100);flush();
  assert(draw_count==draws+1);
  assert(!strcmp(battery_objects[left+SOURCE_OFFSET].label->text,"L100% "));
  assert(!strcmp(battery_objects[right+SOURCE_OFFSET].label->text,"R 43% "));
  battery(left,0);flush();assert(battery_objects[left+SOURCE_OFFSET].label->hidden);
  battery(left,99);flush();assert(!battery_objects[left+SOURCE_OFFSET].label->hidden);
  /* A side change and a battery report must also survive coalescing. */
  key(left,2,true);battery(left,98);flush();
  assert(!strcmp(battery_objects[left+SOURCE_OFFSET].label->text,"R 98% "));
  assert(side_for_position(0)=='L' && side_for_position(3)=='R');
  selected=-1;assert(side_for_position(0)=='?');selected=1;assert(side_for_position(0)=='?');
  selected=0;
#if CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY
  assert(!strcmp(battery_objects[0].label->text,"D 90% "));
  draws=draw_count;usb_powered=false;send((zmk_event_t){.type=4});flush();
  assert(draw_count==draws+1 && !battery_objects[0].rendered.usb_present);
  send((zmk_event_t){.type=3,.own={75}});flush();
  assert(!strcmp(battery_objects[0].label->text,"D 75% "));
#endif
  return 0;
}
'''

MODIFIER_STUB = r'''
#define MOD_LCTL 1
#define MOD_LSFT 2
#define MOD_LALT 4
#define MOD_LGUI 8
#define MOD_RCTL 16
#define MOD_RSFT 32
#define MOD_RALT 64
#define MOD_RGUI 128
typedef int lv_img_dsc_t;
#define LV_IMG_DECLARE(name) static const lv_img_dsc_t name=0
typedef int lv_anim_t;
typedef int lv_style_t;
typedef struct {int x,y;} lv_point_precise_t;
#define LV_ALIGN_TOP_LEFT 0
#define LV_ALIGN_OUT_BOTTOM_LEFT 0
#define lv_obj_set_y(...) ((void)0)
#define lv_anim_init(...) ((void)0)
#define lv_anim_set_var(...) ((void)0)
#define lv_anim_set_duration(...) ((void)0)
#define lv_anim_set_exec_cb(...) ((void)0)
#define lv_anim_set_path_cb(...) ((void)0)
#define lv_anim_set_values(...) ((void)0)
static unsigned animation_count;
static void animate(void){assert(!event_context && !mutex_locked);animation_count++;}
#define lv_anim_start(...) animate()
#define lv_img_create lv_obj_create
#define lv_line_create lv_obj_create
#define lv_img_set_src(...) ((void)0)
#define lv_style_init(...) ((void)0)
#define lv_style_set_line_width(...) ((void)0)
#define lv_line_set_points(...) ((void)0)
#define lv_obj_add_style(...) ((void)0)
static uint8_t active_mods;
static uint8_t zmk_hid_get_explicit_mods(void){return active_mods;}
'''
MODIFIER_TEST = r'''
#include "modifiers.c"
static void key_with_mods(uint8_t mods){
  active_mods=mods;zmk_event_t e={0};event_context=true;
  assert(widget_modifiers_listener(&e)==ZMK_EV_EVENT_BUBBLE);
  event_context=false;
}
int main(void){
  struct zmk_widget_modifiers widget={0};
  zmk_widget_modifiers_init(&widget,NULL);display_initialized=true;
  for(int i=0;i<1000;i++)key_with_mods(0);
  assert(submissions==0 && animation_count==0);
  key_with_mods(MOD_LCTL);assert(animation_count==0);flush();
  assert(ms_control.is_active && animation_count==2);
  for(int i=0;i<1000;i++)key_with_mods(MOD_LCTL);
  assert(submissions==1 && animation_count==2);
  key_with_mods(MOD_LSFT);key_with_mods(MOD_LCTL);flush();
  assert(ms_control.is_active && !ms_shift.is_active && animation_count==2);
  key_with_mods(MOD_RCTL);flush();assert(ms_control.is_active && animation_count==2);
  key_with_mods(0);flush();assert(!ms_control.is_active && animation_count==4);
  return 0;
}
'''

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cc', default='gcc')
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='cornix-battery-', ignore_cleanup_errors=True) as tmp:
    folder = Path(tmp)
    (folder / 'stub.h').write_text(STUB)
    headers = ['lvgl.h', 'zephyr/kernel.h', 'zephyr/bluetooth/services/bas.h',
               'zephyr/logging/log.h', 'zmk/battery.h', 'zmk/ble.h', 'zmk/display.h',
               'zmk/events/battery_state_changed.h', 'zmk/events/position_state_changed.h',
               'zmk/physical_layouts.h', 'zmk/events/usb_conn_state_changed.h',
               'zmk/event_manager.h', 'zmk/usb.h', 'zmk/hid.h',
               'zmk/events/keycode_state_changed.h', 'dt-bindings/zmk/modifiers.h']
    for header in headers:
        path = folder / header
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('#include "stub.h"\n')
    (folder / 'test.c').write_text(TEST)
    for own_battery in [0, 1]:
        exe = folder / 'test.exe'
        command = [args.cc] + (['cc'] if Path(args.cc).stem == 'zig' else [])
        subprocess.run(command + ['-std=gnu11', '-Werror=implicit-function-declaration',
                        f'-DCONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY={own_battery}',
                        '-I', str(folder), '-I', str(ROOT / 'boards/shields/cornix_dongle_display/widgets'),
                        str(folder / 'test.c'), '-o', str(exe)], check=True)
        for order in [[], ['reverse']]:
            subprocess.run([str(exe), *order], check=True)
    (folder / 'stub.h').write_text(STUB + MODIFIER_STUB)
    (folder / 'test.c').write_text(MODIFIER_TEST)
    for mac_modifiers in [0, 1]:
        subprocess.run(command + ['-std=gnu11', '-Werror=implicit-function-declaration',
                        f'-DCONFIG_ZMK_DONGLE_DISPLAY_MAC_MODIFIERS={mac_modifiers}',
                        '-I', str(folder), '-I', str(ROOT / 'boards/shields/cornix_dongle_display/widgets'),
                        str(folder / 'test.c'), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
print('PASS: both source orders, L/R labels, unchanged-event filtering, per-row redraws, coalesced A/B/A updates, display-thread rendering, invalid events and optional dongle battery')
print('PASS: Windows/Mac modifiers, unchanged key events, left/right modifiers and coalesced changes')
