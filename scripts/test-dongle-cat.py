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
typedef struct {uint8_t state;} zmk_event_t;
struct zmk_wpm_state_changed {uint8_t state;};
static const struct zmk_wpm_state_changed *as_zmk_wpm_state_changed(const zmk_event_t *e) {return (const void *)e;}
static int test_wpm;
static int zmk_wpm_get_state(void) {return test_wpm;}
#define ZMK_DISPLAY_WIDGET_LISTENER(name,type,cb,get) static void name##_init(void) {}
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
int main(void) {
  struct zmk_widget_bongo_cat a={0}, b={0};
  zmk_widget_bongo_cat_init(&a,0);
  assert(a.animation_state==0 && a.obj->starts==1 && a.obj->count==1 && a.obj->repeat==0);
  set_animation(&a,4); assert(a.obj->starts==1);
  set_animation(&a,5); assert(a.obj->count==SALARY_WORK_COUNT && a.obj->duration==SALARY_WORK_COUNT*200);
  unsigned starts=a.obj->starts; set_animation(&a,29); assert(a.obj->starts==starts);
  set_animation(&a,30); assert(a.obj->duration==SALARY_WORK_COUNT*100);
  set_animation(&a,69); assert(a.animation_state==2);
  set_animation(&a,70); assert(a.obj->count==SALARY_SNACK_COUNT && a.obj->repeat==LV_ANIM_REPEAT_INFINITE);
  set_animation(&a,0); assert(a.obj->count==1 && a.obj->repeat==0);
  test_wpm=90; zmk_widget_bongo_cat_init(&b,0); assert(b.animation_state==3 && a.animation_state==0);
  assert(bongo_cat_wpm_status_get_state(0).wpm==90);
  zmk_event_t event={12}; assert(bongo_cat_wpm_status_get_state(&event).wpm==12);
  bongo_cat_wpm_status_update_cb((struct bongo_cat_wpm_status_state){15});
  assert(a.animation_state==1 && b.animation_state==1);
  check_images(salary_sleep_frames,SALARY_SLEEP_COUNT);
  check_images(salary_work_frames,SALARY_WORK_COUNT);
  check_images(salary_snack_frames,SALARY_SNACK_COUNT);
  return 0;
}
'''

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cc', default='cc')
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='cornix-cat-', ignore_cleanup_errors=True) as tmp:
    folder = Path(tmp)
    (folder / 'stub.h').write_text(STUB)
    for header in ['lvgl.h','zephyr/kernel.h','zmk/display.h','zmk/event_manager.h','zmk/events/wpm_state_changed.h','zmk/wpm.h']:
        p = folder / header
        p.parent.mkdir(exist_ok=True, parents=True)
        p.write_text('#include "stub.h"\n')
    (folder / 'test.c').write_text(TEST)
    exe = folder / 'test.exe'
    command = [args.cc] + (['cc'] if Path(args.cc).stem == 'zig' else [])
    subprocess.run(command + ['-std=gnu11','-Wall','-Wextra','-Werror','-I',str(folder),'-I',str(WIDGETS),str(folder/'test.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: initial image, WPM boundaries, stable playback, idle stop, independent widgets, event fallback, 29 frame headers/palettes/sizes.')
