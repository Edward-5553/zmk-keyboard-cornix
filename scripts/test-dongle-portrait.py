"""Check the actual portrait flush adapter, including partial updates and polarity."""
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
#include <string.h>
#include <errno.h>
#define PIXEL_FORMAT_MONO10 1
#define PIXEL_FORMAT_MONO01 2
#define SCREEN_INFO_MONO_VTILED 1
#define SCREEN_INFO_MONO_MSB_FIRST 2
#define SCREEN_INFO_X_ALIGNMENT_WIDTH 4
#define LV_COLOR_FORMAT_I1 1
#define LOG_MODULE_DECLARE(...)
#define LOG_ERR(...) ((void)0)
struct device {int unused;};
static struct device device;
#define DT_CHOSEN(...) 0
#define DEVICE_DT_GET(...) (&device)
struct display_capabilities {int x_resolution,y_resolution,screen_info,current_pixel_format;};
static struct display_capabilities capabilities={128,64,SCREEN_INFO_MONO_VTILED,PIXEL_FORMAT_MONO10};
struct display_buffer_descriptor {size_t buf_size;uint16_t width,pitch,height;bool frame_incomplete;};
typedef struct {int x1,y1,x2,y2;} lv_area_t;
typedef struct lv_display {int width,height;void (*flush)(struct lv_display *,const lv_area_t *,uint8_t *);} lv_display_t;
static lv_display_t screen;
static unsigned writes,ready;
static bool last=true;
static int write_error;
static uint8_t panel_pixels[1024];
static uint16_t write_x,write_y;
static struct display_buffer_descriptor write_desc;
static void display_get_capabilities(const struct device *d,struct display_capabilities *c){(void)d;*c=capabilities;}
static lv_display_t *lv_display_get_default(void){return &screen;}
static void lv_display_set_resolution(lv_display_t *d,int w,int h){d->width=w;d->height=h;}
static void lv_display_set_flush_wait_cb(lv_display_t *d,void *cb){(void)d;assert(!cb);}
static void lv_display_set_flush_cb(lv_display_t *d,void (*cb)(lv_display_t *,const lv_area_t *,uint8_t *)){d->flush=cb;}
static uint32_t lv_draw_buf_width_to_stride(uint32_t w,int cf){(void)cf;return ((w+7)/8+3)&~3u;}
static bool lv_display_flush_is_last(lv_display_t *d){(void)d;return last;}
static void lv_display_flush_ready(lv_display_t *d){(void)d;ready++;}
static int display_write(const struct device *d,uint16_t x,uint16_t y,const struct display_buffer_descriptor *desc,const void *buf){
  (void)d;writes++;write_x=x;write_y=y;write_desc=*desc;
  assert(x+desc->width<=128 && y+desc->height<=64 && y%8==0 && desc->height%8==0);
  assert(desc->pitch==desc->width && desc->buf_size==desc->width*desc->height/8);
  for(int page=0;page<desc->height/8;page++)
    memcpy(panel_pixels+(y/8+page)*128+x,(const uint8_t *)buf+page*desc->width,desc->width);
  return write_error;
}
'''
TEST = r'''
#include "portrait_display.c"
static bool pattern(int x,int y){return ((x*17+y*11)%19)<7;}
static void paint(int x,int y,int w,int h,bool value_pattern){
  uint8_t buffer[1032];memset(buffer,0xa5,sizeof(buffer));
  size_t stride=lv_draw_buf_width_to_stride(w,LV_COLOR_FORMAT_I1);
  memset(buffer+8,0,stride*h);
  for(int j=0;j<h;j++)for(int i=0;i<w;i++)
    if(value_pattern ? pattern(x+i,y+j) : true)buffer[8+j*stride+i/8]|=0x80>>(i%8);
  uint8_t original[sizeof(buffer)];memcpy(original,buffer,sizeof(buffer));
  lv_area_t area={x,y,x+w-1,y+h-1};
  unsigned before=ready;screen.flush(&screen,&area,buffer);
  assert(ready==before+1 && !memcmp(buffer,original,sizeof(buffer)));
  assert(write_x==128-y-h && write_y==x && write_desc.width==h && write_desc.height==w);
}
static void check(bool mono10){
  for(int y=0;y<128;y++)for(int x=0;x<64;x++){
    /* Independently sample the physical panel at the expected global coordinate. */
    int px=127-y,py=x;
    bool bit=(panel_pixels[px+128*(py/8)]>>(py%8))&1;
    assert(bit==(mono10?pattern(x,y):!pattern(x,y)));
  }
}
int main(void){
  assert(cornix_portrait_display_init()==0 && screen.width==64 && screen.height==128);
  for(int mode=0;mode<2;mode++){
    bool mono10=mode==0;capabilities.current_pixel_format=mono10?PIXEL_FORMAT_MONO10:PIXEL_FORMAT_MONO01;
    paint(0,0,64,128,true);check(mono10);
    /* Tile the entire display using differently sized, offset dirty rectangles.
     * A 24-pixel row requires padding, exercising the actual byte stride. */
    memset(panel_pixels,0x5a,sizeof(panel_pixels));
    for(int y=0;y<128;y+=8){
      last=false;paint(0,y,24,8,true);assert(write_desc.frame_incomplete);
      paint(24,y,24,8,true);
      last=true;paint(48,y,16,8,true);assert(!write_desc.frame_incomplete);
    }
    check(mono10);
    uint8_t before[1024];memcpy(before,panel_pixels,sizeof(before));
    paint(16,48,32,32,false); /* Cat-sized update must preserve every outside pixel. */
    for(int py=0;py<64;py++)for(int px=0;px<128;px++){
      int bit=1<<(py%8),idx=px+128*(py/8);
      if(px>=48 && px<80 && py>=16 && py<48)assert(!!(panel_pixels[idx]&bit)==mono10);
      else assert((panel_pixels[idx]&bit)==(before[idx]&bit));
    }
  }
  uint8_t buffer[1032]={0};unsigned before=writes;
  lv_area_t invalid={1,0,8,7};screen.flush(&screen,&invalid,buffer);assert(writes==before);
  invalid=(lv_area_t){0,0,71,7};screen.flush(&screen,&invalid,buffer);assert(writes==before);
  write_error=-EIO;paint(0,0,8,8,true); /* Failed I/O still releases LVGL. */
  capabilities.x_resolution=64;assert(cornix_portrait_display_init()==-ENOTSUP);
  capabilities.x_resolution=128;capabilities.screen_info|=SCREEN_INFO_MONO_MSB_FIRST;
  assert(cornix_portrait_display_init()==-ENOTSUP);
  return 0;
}
'''

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cc', default='cc')
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='cornix-portrait-', ignore_cleanup_errors=True) as tmp:
    folder = Path(tmp)
    (folder / 'stub.h').write_text(STUB)
    for name in ['lvgl.h', 'zephyr/device.h', 'zephyr/devicetree.h',
                 'zephyr/drivers/display.h', 'zephyr/logging/log.h']:
        path = folder / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('#include "stub.h"\n')
    (folder / 'test.c').write_text(TEST)
    exe = folder / 'test.exe'
    command = [args.cc] + (['cc'] if Path(args.cc).stem == 'zig' else [])
    subprocess.run(command + ['-std=gnu11', '-Wall', '-Wextra', '-Werror',
                              '-I', str(folder), '-I', str(ROOT / 'boards/shields/cornix_dongle_display'),
                              str(folder / 'test.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: portrait flush, 90-degree mapping, both polarities, padded strides, partial updates, bounds and I/O errors')
