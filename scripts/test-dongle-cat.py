"""Host-check the actual Salary Cat widget and generated LVGL image data."""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
WIDGETS = ROOT / 'boards/shields/cornix_dongle_display/widgets'
STUB = r'''
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
typedef struct node { struct node *next; } sys_snode_t;
typedef struct { sys_snode_t *head; } sys_slist_t;
#define SYS_SLIST_STATIC_INIT(x) {0}
static void sys_slist_append(sys_slist_t *s, sys_snode_t *n) { n->next=s->head; s->head=n; }
#define SYS_SLIST_FOR_EACH_CONTAINER(s,w,member) for(sys_snode_t *n=(s)->head;n && ((w)=(void *)((char *)n - offsetof(__typeof__(*(w)),member)),1);n=n->next)
typedef struct { struct { unsigned magic,cf,w,h,stride; } header; unsigned data_size; const uint8_t *data; } lv_image_dsc_t;
typedef struct { const void **src; unsigned count,duration,repeat,starts; } lv_obj_t;
#define LV_IMAGE_HEADER_MAGIC 0x19
#define LV_COLOR_FORMAT_I1 7
#define LV_ANIM_REPEAT_INFINITE 0xffffffff
static lv_obj_t objects[4]; static unsigned object_count;
static lv_obj_t *lv_animimg_create(lv_obj_t *parent) { (void)parent; return &objects[object_count++]; }
static void lv_obj_center(lv_obj_t *o) { (void)o; }
static void lv_animimg_set_src(lv_obj_t *o,const void **s,unsigned n) {o->src=s;o->count=n;}
static void lv_animimg_set_duration(lv_obj_t *o,unsigned n) {o->duration=n;}
static void lv_animimg_set_repeat_count(lv_obj_t *o,unsigned n) {o->repeat=n;}
static void lv_animimg_start(lv_obj_t *o) {o->starts++;}
struct k_spinlock {bool held;};
typedef int k_spinlock_key_t;
static k_spinlock_key_t k_spin_lock(struct k_spinlock *l){assert(!l->held);l->held=true;return 0;}
static void k_spin_unlock(struct k_spinlock *l,k_spinlock_key_t k){(void)k;assert(l->held);l->held=false;}
static int64_t test_now;
static int64_t k_uptime_get(void){return test_now;}
typedef struct lv_timer {void (*callback)(struct lv_timer *);unsigned period;} lv_timer_t;
static lv_timer_t timer;
static unsigned timer_count;
static lv_timer_t *lv_timer_create(void (*cb)(lv_timer_t *),unsigned ms,void *data){(void)data;timer=(lv_timer_t){cb,ms};timer_count++;return &timer;}
struct zmk_position_state_changed {uint8_t source;uint32_t position;bool state;int64_t timestamp;};
typedef struct {bool is_position;struct zmk_position_state_changed pos;} zmk_event_t;
static const struct zmk_position_state_changed *as_zmk_position_state_changed(const zmk_event_t *e){return e->is_position?&e->pos:NULL;}
#define ZMK_EV_EVENT_BUBBLE 0
#define ZMK_LISTENER(name,cb) static int (*name##_listener)(const zmk_event_t *)=cb
#define ZMK_SUBSCRIPTION(a,b)
'''
TEST = r'''
#include "bongo_cat.c"
#include "salary_cat_images.c"
static void check_images(const lv_image_dsc_t **frames,unsigned count) {
  for(unsigned i=0;i<count;i++) {
    const lv_image_dsc_t *f=frames[i];
    assert(f->header.magic==LV_IMAGE_HEADER_MAGIC && f->header.cf==LV_COLOR_FORMAT_I1);
    assert(f->header.w==32 && f->header.h==32 && f->header.stride==4);
    assert(f->data_size==136 && f->data[0]==255 && f->data[3]==255 && f->data[4]==0 && f->data[7]==255);
  }
}
static void event_at(int64_t now,bool down,unsigned source){
  test_now=now;
  zmk_event_t event={.is_position=true,.pos={.source=source,.state=down,.timestamp=now}};
  unsigned starts=objects[0].starts;
  assert(widget_bongo_cat_listener(&event)==ZMK_EV_EVENT_BUBBLE);
  assert(objects[0].starts==starts); /* Event thread never calls LVGL. */
}
static void poll_at(int64_t now){test_now=now;timer.callback(&timer);}
int main(void) {
  struct zmk_widget_bongo_cat a={0}, b={0};
  zmk_widget_bongo_cat_init(&a,0);
  assert(a.animation_state==0 && a.obj->starts==1 && a.obj->count==SALARY_IDLE_COUNT);
  assert(a.obj->src==(const void **)salary_idle_frames && a.obj->repeat==LV_ANIM_REPEAT_INFINITE);
  assert(SALARY_IDLE_COUNT>1 && a.obj->duration==SALARY_IDLE_COUNT*100);
  assert(timer_count==1 && timer.period==50);
  poll_at(0); assert(a.obj->starts==1);
  event_at(0,true,0); /* Uptime zero is a real first press. */
  event_at(500,true,1);event_at(1000,true,0);event_at(1500,true,1);
  event_at(2000,true,0);event_at(2500,true,1);event_at(2999,true,0);
  poll_at(2999);assert(a.animation_state==0 && a.obj->starts==1);
  event_at(3000,true,1);poll_at(3000);
  assert(a.animation_state==1 && a.obj->starts==2);
  assert(a.obj->src==(const void **)salary_error_frames && a.obj->count==SALARY_ERROR_COUNT);
  assert(SALARY_ERROR_COUNT>1 && a.obj->duration==SALARY_ERROR_COUNT*100 && a.obj->repeat==LV_ANIM_REPEAT_INFINITE);
  unsigned starts=a.obj->starts;
  event_at(3400,true,0);poll_at(3400);assert(a.obj->starts==starts);
  zmk_widget_bongo_cat_init(&b,0);assert(b.animation_state==1 && timer_count==1);
  event_at(3800,false,1); /* Releases must not extend the inactivity deadline. */
  poll_at(4399);assert(a.animation_state==1 && a.obj->starts==starts);
  poll_at(4400);assert(a.animation_state==0 && b.animation_state==0 && a.obj->starts==starts+1);
  assert(a.obj->src==(const void **)salary_idle_frames && a.obj->repeat==LV_ANIM_REPEAT_INFINITE);
  starts=a.obj->starts;poll_at(4500);assert(a.obj->starts==starts);
  /* A 2.9-second burst must not switch just because wall time reaches 3 seconds. */
  for(int64_t t=5000;t<=7500;t+=500)event_at(t,true,0);
  event_at(7900,true,1);poll_at(8000);assert(a.animation_state==0);
  poll_at(8900);assert(!typing_active());
  /* A held key and its later release never qualify as continuous typing. */
  event_at(10000,true,0);poll_at(13000);assert(a.animation_state==0);
  event_at(14000,false,0);poll_at(14000);assert(a.animation_state==0);
  /* A gap of exactly 1s resets even if the display timer was delayed. */
  event_at(15000,true,0);event_at(15999,true,1);event_at(16998,true,0);
  event_at(17998,true,1);poll_at(18000);assert(a.animation_state==0);
  assert(activity.first_press==17998);
  /* Long uptimes and a new uninterrupted streak use 64-bit time correctly. */
  int64_t base=INT64_C(1)<<33;
  for(int i=0;i<=6;i++)event_at(base+i*500,true,i%2);
  poll_at(base+3000);assert(a.animation_state==1 && b.animation_state==1);
  zmk_event_t unrelated={0};test_now=base+3500;
  assert(widget_bongo_cat_listener(&unrelated)==ZMK_EV_EVENT_BUBBLE);
  poll_at(base+4000);assert(a.animation_state==0 && b.animation_state==0);
  check_images(salary_idle_frames,SALARY_IDLE_COUNT);
  check_images(salary_error_frames,SALARY_ERROR_COUNT);
  return 0;
}
'''

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cc', default='cc')
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='cornix-cat-', ignore_cleanup_errors=True) as tmp:
    folder = Path(tmp)
    (folder / 'stub.h').write_text(STUB)
    for header in ['lvgl.h','zephyr/kernel.h','zmk/event_manager.h','zmk/events/position_state_changed.h']:
        p = folder / header
        p.parent.mkdir(exist_ok=True, parents=True)
        p.write_text('#include "stub.h"\n')
    (folder / 'test.c').write_text(TEST)
    exe = folder / 'test.exe'
    command = [args.cc] + (['cc'] if Path(args.cc).stem == 'zig' else [])
    subprocess.run(command + ['-std=gnu11','-Wall','-Wextra','-Werror','-I',str(folder),'-I',str(WIDGETS),str(folder/'test.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: 3s continuous presses, 1s idle/reset, interrupted bursts, held keys/releases, both halves, stable looping, display-thread updates, shared timer, 64-bit uptime and image metadata.')
