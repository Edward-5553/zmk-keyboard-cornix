"""Compile the real battery widget against host stubs; no Zephyr SDK required."""
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
#define lv_canvas_fill_bg(...) ((void)0)
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
static void lv_label_set_text_fmt(lv_obj_t *o,const char *f,...){va_list a;va_start(a,f);vsnprintf(o->text,sizeof(o->text),f,a);va_end(a);}
struct zmk_position_state_changed {uint8_t source;uint32_t position;bool state;};
struct zmk_peripheral_battery_state_changed {uint8_t source,state_of_charge;};
struct zmk_battery_state_changed {uint8_t state_of_charge;};
typedef struct {int type;union {struct zmk_position_state_changed pos;struct zmk_peripheral_battery_state_changed bat;struct zmk_battery_state_changed own;};} zmk_event_t;
static const struct zmk_position_state_changed *as_zmk_position_state_changed(const zmk_event_t *e){return e->type==1?&e->pos:NULL;}
static const struct zmk_peripheral_battery_state_changed *as_zmk_peripheral_battery_state_changed(const zmk_event_t *e){return e->type==2?&e->bat:NULL;}
static const struct zmk_battery_state_changed *as_zmk_battery_state_changed(const zmk_event_t *e){return e->type==3?&e->own:NULL;}
static int zmk_battery_state_of_charge(void){return 90;}
static bool zmk_usb_is_powered(void){return true;}
struct zmk_key_physical_attrs {int16_t x;};
struct zmk_physical_layout {const struct zmk_key_physical_attrs *keys;size_t keys_len;};
static const struct zmk_key_physical_attrs keys[]={{0},{600},{750},{1350},{700}};
static const struct zmk_physical_layout layout={keys,5};
static const struct zmk_physical_layout *layouts[]={&layout};
static int selected;
static size_t zmk_physical_layouts_get_list(const struct zmk_physical_layout *const **p){*p=layouts;return 1;}
static int zmk_physical_layouts_get_selected(void){return selected;}
#define ZMK_DISPLAY_WIDGET_LISTENER(name,type,cb,get) static void name##_init(void){cb(get(NULL));}
#define ZMK_SUBSCRIPTION(...)
'''
TEST = r'''
#include "battery_status.c"
static struct battery_snapshot battery(int source,int level){zmk_event_t e={.type=2,.bat={source,level}};return battery_status_get_state(&e);}
static struct battery_snapshot key(int source,int pos,bool down){zmk_event_t e={.type=1,.pos={source,pos,down}};return battery_status_get_state(&e);}
int main(int argc,char **argv){
  int left=argc>1?1:0,right=1-left;
  struct zmk_widget_dongle_battery_status widget={0};
  zmk_widget_dongle_battery_status_init(&widget,NULL);
  assert(battery_objects[SOURCE_OFFSET].label->hidden);
  battery(left,81);
  struct battery_snapshot s=battery(right,42);
  battery_status_update_cb(s);
  assert(!strcmp(battery_objects[left+SOURCE_OFFSET].label->text,"? 81% "));
  assert(!strcmp(battery_objects[right+SOURCE_OFFSET].label->text,"? 42% "));
  key(left,0,false); key(255,0,true);key(left,99,true);key(left,4,true);
  s=battery_status_get_state(NULL);assert(!s.sources[left+SOURCE_OFFSET].side);
  key(left,1,true);s=key(right,2,true);battery_status_update_cb(s);
  assert(!strcmp(battery_objects[left+SOURCE_OFFSET].label->text,"L 81% "));
  assert(!strcmp(battery_objects[right+SOURCE_OFFSET].label->text,"R 42% "));
  s=battery(left,100);battery_status_update_cb(s);
  assert(!strcmp(battery_objects[left+SOURCE_OFFSET].label->text,"L100% "));
  assert(s.sources[right+SOURCE_OFFSET].level==42);
  s=battery(9,20);assert(s.sources[left+SOURCE_OFFSET].level==100);
  s=battery(255,20);assert(s.sources[left+SOURCE_OFFSET].level==100);
  assert(s.sources[0].level==(SOURCE_OFFSET?90:(left==0?100:42)));
  assert(side_for_position(0)=='L' && side_for_position(3)=='R');
  selected=-1;assert(side_for_position(0)=='?');selected=1;assert(side_for_position(0)=='?');
  selected=0;
#if CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY
  assert(!strcmp(battery_objects[0].label->text,"D 90% "));
#endif
  return 0;
}
'''

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cc', default='gcc')
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='cornix-battery-') as tmp:
    folder = Path(tmp)
    (folder / 'stub.h').write_text(STUB)
    headers = ['lvgl.h', 'zephyr/kernel.h', 'zephyr/bluetooth/services/bas.h',
               'zephyr/logging/log.h', 'zmk/battery.h', 'zmk/ble.h', 'zmk/display.h',
               'zmk/events/battery_state_changed.h', 'zmk/events/position_state_changed.h',
               'zmk/physical_layouts.h', 'zmk/events/usb_conn_state_changed.h',
               'zmk/event_manager.h', 'zmk/usb.h']
    for header in headers:
        path = folder / header
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('#include "stub.h"\n')
    (folder / 'test.c').write_text(TEST)
    for own_battery in [0, 1]:
        exe = folder / 'test.exe'
        subprocess.run([args.cc, '-std=gnu11', '-Werror=implicit-function-declaration',
                        f'-DCONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY={own_battery}',
                        '-I', str(folder), '-I', str(ROOT / 'boards/shields/cornix_dongle_display/widgets'),
                        str(folder / 'test.c'), '-o', str(exe)], check=True)
        for order in [[], ['reverse']]:
            subprocess.run([str(exe), *order], check=True)
print('PASS: both source orders, unknown/learned sides, coalesced readings, invalid events, optional dongle battery')
