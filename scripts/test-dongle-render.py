"""Render the actual widgets with real LVGL in CI; assert bounds and save SVG previews."""
import argparse
import ast
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
DISPLAY = ROOT / 'boards/shields/cornix_dongle_display'


def literal(path, name):
    for node in ast.parse(path.read_text(encoding='utf-8')).body:
        if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == name for t in node.targets):
            return ast.literal_eval(node.value)
    raise ValueError(name)


EXTRA = r'''
#define CONFIG_ZMK_BATTERY 1
#define CONFIG_ZMK_BLE 1
#define CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY 0
#define CONFIG_ZMK_DONGLE_DISPLAY_MAC_MODIFIERS 0
#define CONFIG_ZMK_DONGLE_DISPLAY_BONGO_CAT 1
#define CONFIG_ZMK_DONGLE_DISPLAY_MODIFIERS 1
#define CONFIG_ZMK_DONGLE_DISPLAY_LAYER 1
#define CONFIG_ZMK_DONGLE_DISPLAY_WPM 0
#define CONFIG_ZMK_HID_INDICATORS 1
#define CONFIG_ZMK_DONGLE_DISPLAY_LAYER_NAME_SCROLL_WIDTH 60
#define CONFIG_ZMK_DONGLE_DISPLAY_LAYER_TEXT_ALIGN "center"
#define MOD_LCTL 1
#define MOD_LSFT 2
#define MOD_LALT 4
#define MOD_LGUI 8
#define MOD_RCTL 16
#define MOD_RSFT 32
#define MOD_RALT 64
#define MOD_RGUI 128
static uint8_t active_mods;
static uint8_t zmk_hid_get_explicit_mods(void){return active_mods;}
static uint8_t zmk_hid_indicators_get_current_profile(void){return 0;}
struct zmk_hid_indicators_changed {uint8_t indicators;};
static const struct zmk_hid_indicators_changed *as_zmk_hid_indicators_changed(const zmk_event_t *e){(void)e;return NULL;}
enum zmk_transport {ZMK_TRANSPORT_NONE,ZMK_TRANSPORT_USB,ZMK_TRANSPORT_BLE};
struct zmk_endpoint_instance {enum zmk_transport transport;};
static struct zmk_endpoint_instance zmk_endpoint_get_selected(void){return (struct zmk_endpoint_instance){ZMK_TRANSPORT_USB};}
static enum zmk_transport zmk_endpoint_get_preferred_transport(void){return ZMK_TRANSPORT_USB;}
static bool zmk_usb_is_hid_ready(void){return true;}
static int zmk_ble_active_profile_index(void){return 0;}
static bool zmk_ble_active_profile_is_connected(void){return false;}
static bool zmk_ble_active_profile_is_open(void){return true;}
static uint8_t layer_index;
static uint8_t zmk_keymap_highest_layer_active(void){return layer_index;}
static const char *zmk_keymap_layer_name(uint8_t layer){return layer==0?"Base":"Fn/Symbol";}
struct k_spinlock {bool held;};
typedef int k_spinlock_key_t;
static k_spinlock_key_t k_spin_lock(struct k_spinlock *l){assert(!l->held);l->held=true;return 0;}
static void k_spin_unlock(struct k_spinlock *l,k_spinlock_key_t k){(void)k;assert(l->held);l->held=false;}
static int64_t test_now;
static int64_t k_uptime_get(void){return test_now;}
#define ZMK_DISPLAY_WIDGET_LISTENER(name,type,cb,get_state) \
    static void name##_init(void){cb(get_state(NULL));}
'''

TEST = r'''
#define widgets battery_widgets
#include "widgets/battery_status.c"
#undef widgets
#define widgets cat_widgets
#include "widgets/bongo_cat.c"
#undef widgets
#define widgets modifier_widgets
#include "widgets/modifiers.c"
#undef widgets
#define widgets output_widgets
#include "widgets/output_status.c"
#undef widgets
#define widgets layer_widgets
#include "widgets/layer_status.c"
#undef widgets
#define widgets indicator_widgets
#include "widgets/hid_indicators.c"
#undef widgets
#include "widgets/salary_cat_images.c"
#include "widgets/modifiers_sym.c"
#include "widgets/output_status_sym.c"
#include "custom_status_screen.c"

/* Pixel rotation is exercised separately by test-dongle-portrait.py. */
int cornix_portrait_display_init(void){return 0;}
static uint8_t pixels[1024];
static unsigned frames;
static void capture(lv_display_t *display,const lv_area_t *area,uint8_t *data){
  assert(area->x1==0 && area->y1==0 && area->x2==63 && area->y2==127);
  memcpy(pixels,data+8,sizeof(pixels));frames++;lv_display_flush_ready(display);
}
static unsigned dark(int x,int y,int w,int h){
  unsigned count=0;
  for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++)count+=!(pixels[j*8+i/8]&(0x80>>(i%8)));
  return count;
}
static void inside_impl(lv_obj_t *obj,int x,int y,int w,int h,const char *name){
  lv_area_t a;lv_obj_get_coords(obj,&a);
  if(!(a.x1>=x && a.y1>=y && a.x2<x+w && a.y2<y+h))
    fprintf(stderr,"%s bounds [%d,%d,%d,%d] expected within [%d,%d,%d,%d]\n",name,(int)a.x1,(int)a.y1,(int)a.x2,(int)a.y2,x,y,x+w-1,y+h-1);
  assert(a.x1>=x && a.y1>=y && a.x2<x+w && a.y2<y+h);
}
#define inside(obj,x,y,w,h) inside_impl(obj,x,y,w,h,#obj)
static void snapshot(lv_display_t *display,const char *path){
  lv_obj_update_layout(lv_screen_active());
  inside(output_status_widget.obj,0,0,64,22);
  inside(dongle_battery_status_widget.obj,0,22,64,20);
  inside(bongo_cat_widget.obj,16,46,32,32);
  inside(layer_status_widget.obj,2,82,60,8);
  inside(hid_indicators_widget.obj,0,100,64,8);
  inside(modifiers_widget.obj,1,109,61,19);
  for(int row=0;row<2;row++){
    inside(battery_objects[row].label,0,22+10*row,54,8);
    inside(battery_objects[row].symbol,58,22+10*row,5,8);
  }
  unsigned before=frames;lv_obj_invalidate(lv_screen_active());lv_refr_now(display);assert(frames>before);
  for(int row=0;row<2;row++){
    assert(dark(0,22+10*row,8,8)>5); /* Actual L/R ink, not just a label object. */
    assert(dark(16,22+10*row,32,8)>5); /* Actual percent/placeholder glyphs. */
  }
  FILE *file=fopen(path,"w");assert(file);
  fputs("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"256\" height=\"512\" viewBox=\"0 0 64 128\"><rect width=\"64\" height=\"128\" fill=\"white\"/><path fill=\"black\" d=\"",file);
  for(int y=0;y<128;y++)for(int x=0;x<64;x++)if(dark(x,y,1,1))fprintf(file,"M%d %dh1v1h-1z",x,y);
  fputs("\"/></svg>\n",file);fclose(file);
}
int main(void){
  lv_init();
  lv_display_t *display=lv_display_create(64,128);
  static uint8_t buffer[1032];
  lv_display_set_color_format(display,LV_COLOR_FORMAT_I1);
  lv_display_set_buffers(display,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_FULL);
  lv_display_set_flush_cb(display,capture);
  lv_obj_t *screen=zmk_display_status_screen();assert(screen);lv_screen_load(screen);
  display_initialized=true;
  snapshot(display,"startup.svg");
  zmk_event_t left={.type=2,.bat={1,81}},right={.type=2,.bat={0,42}};
  widget_dongle_battery_status_listener(&left);widget_dongle_battery_status_listener(&right);flush();
  left=(zmk_event_t){.type=1,.pos={1,0,true}};right=(zmk_event_t){.type=1,.pos={0,2,true}};
  widget_dongle_battery_status_listener(&left);widget_dongle_battery_status_listener(&right);flush();
  assert(!strcmp(lv_label_get_text(battery_objects[0].label),"L  81%"));
  assert(!strcmp(lv_label_get_text(battery_objects[1].label),"R  42%"));
  snapshot(display,"idle.svg");
  test_now=20;widget_bongo_cat_listener(&left);activity_timer_cb(NULL);
  layer_index=1;layer_status_update_cb(layer_status_get_state(NULL));
  hid_indicators_update_cb((struct hid_indicators_state){.hid_indicators=2});
  snapshot(display,"active.svg");
  puts("PASS: real LVGL portrait layout, visible L/R and percentage pixels, widget bounds, idle/active rendering");
  return 0;
}
'''

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--lvgl-dir', type=Path, required=True)
parser.add_argument('--output-dir', type=Path, required=True)
args = parser.parse_args()
lvgl = args.lvgl_dir.resolve()
output = args.output_dir.resolve()
output.mkdir(parents=True, exist_ok=True)
stub = literal(ROOT / 'scripts/test-dongle-battery.py', 'STUB')
start = stub.index('typedef int lv_color_t;')
end = stub.index('struct zmk_position_state_changed')
stub = stub[:start] + '#include <lvgl.h>\nstatic bool event_context, mutex_locked;\n' + stub[end:] + EXTRA
with tempfile.TemporaryDirectory(prefix='cornix-render-') as tmp:
    folder = Path(tmp)
    (folder / 'stub.h').write_text(stub)
    headers = ['zephyr/kernel.h', 'zephyr/bluetooth/services/bas.h', 'zephyr/logging/log.h',
               'zmk/battery.h', 'zmk/ble.h', 'zmk/display.h', 'zmk/physical_layouts.h', 'zmk/event_manager.h',
               'zmk/usb.h', 'zmk/hid.h', 'zmk/hid_indicators.h', 'zmk/endpoints.h', 'zmk/keymap.h',
               'dt-bindings/zmk/modifiers.h']
    headers += [f'zmk/events/{name}.h' for name in ['battery_state_changed', 'position_state_changed',
                'usb_conn_state_changed', 'keycode_state_changed', 'endpoint_changed',
                'ble_active_profile_changed', 'layer_state_changed', 'hid_indicators_changed']]
    for name in headers:
        path = folder / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('#include "stub.h"\n')
    config = (lvgl / 'lv_conf_template.h').read_text()
    config = re.sub(r'^#if 0.*$', '#if 1', config, count=1, flags=re.M)
    config = re.sub(r'(#define LV_COLOR_DEPTH)\s+\d+', r'\1 1', config)
    config = re.sub(r'(#define LV_FONT_UNSCII_8)\s+\d+', r'\1 1', config)
    config = re.sub(r'(#define LV_DRAW_SW_SUPPORT_I1)\s+\d+', r'\1 1', config)
    (folder / 'lv_conf.h').write_text(config)
    (folder / 'test.c').write_text(TEST)
    (folder / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.16)
project(cornix_render C)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON CACHE BOOL "" FORCE)
add_subdirectory("{lvgl.as_posix()}" lvgl)
target_include_directories(lvgl PUBLIC "{folder.as_posix()}")
add_executable(render test.c)
target_include_directories(render PRIVATE "{folder.as_posix()}" "{DISPLAY.as_posix()}")
target_link_libraries(render PRIVATE lvgl m)
''')
    subprocess.run(['cmake', '-S', str(folder), '-B', str(folder / 'build')], check=True)
    subprocess.run(['cmake', '--build', str(folder / 'build'), '--target', 'render', '-j', '2'], check=True)
    subprocess.run([str(folder / 'build/render')], cwd=output, check=True)
