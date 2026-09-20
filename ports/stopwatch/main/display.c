// SPDX-License-Identifier: MIT
#include "display_status.h"
#include "display_hw.h"
#include "orientation.h"
#include "orientation_imu.h"
#include "assets/cat_frames.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if CONFIG_STOPWATCH_DISPLAY
static const char *TAG="display";
extern const lv_font_t stopwatch_font_16;
extern const uint8_t cat_work_start[] asm("_binary_cat_work_bin_start");
extern const uint8_t cat_sleep_start[] asm("_binary_cat_sleep_bin_start");
static lv_image_dsc_t work_frames[CAT_WORK_FRAMES],sleep_frames[CAT_SLEEP_FRAMES];
static lv_obj_t *usb_label,*battery_labels[2],*battery_bars[2],*wpm_label,*cat_image,*layer_label,*message,*layer_dots[5];
static bool display_fault;
static uint16_t *rotation_canvas;
static uint8_t *rotation_dma;
static struct orientation pose;
static bool imu_ready;
static unsigned imu_failures;
static float rotation_angle;
static bool canvas_dirty;
static int dirty_y1=DISPLAY_HEIGHT,dirty_y2=0;
static bool rotation_ready;

static lv_color_t green(void) {return lv_color_hex(0x96d8aa);}
static lv_color_t muted(void) {return lv_color_hex(0x9daaa3);}
static lv_color_t amber(void) {return lv_color_hex(0xeda575);}
static uint32_t tick_ms(void) {return esp_timer_get_time()/1000;}

static void flush(lv_display_t *d,const lv_area_t *area,uint8_t *pixels) {
    if(!display_fault) {
        esp_err_t rc=ESP_OK;
        if(rotation_ready) {
            // LVGL still uses its small internal draw buffer. Accumulate dirty regions in PSRAM.
            int width=area->x2-area->x1+1;
            for(int y=area->y1;y<=area->y2;y++)
                memcpy(rotation_canvas+y*DISPLAY_WIDTH+area->x1,pixels+(y-area->y1)*width*2,width*2);
            int low=area->y1,high=area->y2+1;
            switch((int)orientation_cardinal(rotation_angle)) {
            case 90: low=area->x1;high=area->x2+1;break;
            case 180: low=DISPLAY_HEIGHT-1-area->y2;high=DISPLAY_HEIGHT-area->y1;break;
            case 270: low=DISPLAY_WIDTH-1-area->x2;high=DISPLAY_WIDTH-area->x1;break;
            }
            if(low<dirty_y1)dirty_y1=low;
            if(high>dirty_y2)dirty_y2=high;
            canvas_dirty=true;
        } else {
            unsigned count=(area->x2-area->x1+1)*(area->y2-area->y1+1);
            lv_draw_sw_rgb565_swap(pixels,count);
            rc=display_hw_flush(area->x1,area->y1,area->x2+1,area->y2+1,pixels);
        }
        if(rc!=ESP_OK) {
            ESP_LOGE(TAG,"LCD transfer stopped: %s; BLE/USB remain active",esp_err_to_name(rc));
            display_fault=true;
        }
    }
    lv_display_flush_ready(d);
}
static void present_rotated(void) {
    if(!rotation_ready || !canvas_dirty || display_fault)return;
    canvas_dirty=false;
    int first=dirty_y1,last=dirty_y2;
    dirty_y1=DISPLAY_HEIGHT;dirty_y2=0;
    for(int y=first;y<last;y+=16) {
        int rows=last-y;if(rows>16)rows=16;
        orientation_rows(rotation_canvas,(uint16_t *)rotation_dma,DISPLAY_WIDTH,DISPLAY_HEIGHT,y,rows,rotation_angle);
        lv_draw_sw_rgb565_swap(rotation_dma,DISPLAY_WIDTH*rows);
        esp_err_t rc=display_hw_flush(0,y,DISPLAY_WIDTH,y+rows,rotation_dma);
        if(rc!=ESP_OK){ESP_LOGE(TAG,"Rotated transfer failed: %s",esp_err_to_name(rc));display_fault=true;break;}
        // Yield between stripes; keyboard processing must never wait for this task.
        if((y/16)%8==7)vTaskDelay(pdMS_TO_TICKS(1));
    }
}
static void sample_orientation(uint32_t now) {
    if(!imu_ready)return;
    float x,y,z;
    if(orientation_imu_read(&x,&y,&z)!=ESP_OK) {
        if(++imu_failures>=5){imu_ready=false;ESP_LOGW(TAG,"IMU read failed repeatedly; retaining last angle");}
        return;
    }
    imu_failures=0;
#ifdef CONFIG_STOPWATCH_ORIENTATION_INVERT_X
    x=-x;
#endif
#ifdef CONFIG_STOPWATCH_ORIENTATION_INVERT_Y
    y=-y;
#endif
    if(orientation_update(&pose,x,y,z,now)) {
        rotation_angle=orientation_cardinal(pose.angle+CONFIG_STOPWATCH_ORIENTATION_OFFSET);
        dirty_y1=0;dirty_y2=DISPLAY_HEIGHT;
        canvas_dirty=true;
    }
}
static void round_area(lv_event_t *e) {
    lv_area_t *area=lv_event_get_param(e);
    area->x1&=~1;area->y1&=~1;
    area->x2|=1;area->y2|=1;
}
static lv_obj_t *label(int x,int y,int width,const lv_font_t *font,lv_text_align_t align,lv_color_t color) {
    lv_obj_t *obj=lv_label_create(lv_screen_active());
    lv_obj_set_pos(obj,x,y);lv_obj_set_width(obj,width);
    lv_label_set_long_mode(obj,LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(obj,font,0);lv_obj_set_style_text_align(obj,align,0);
    lv_obj_set_style_text_color(obj,color,0);
    return obj;
}
static lv_obj_t *dot(int x,int y,int size,lv_color_t color) {
    lv_obj_t *obj=lv_obj_create(lv_screen_active());lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj,x,y);lv_obj_set_size(obj,size,size);
    lv_obj_set_style_radius(obj,LV_RADIUS_CIRCLE,0);lv_obj_set_style_bg_opa(obj,LV_OPA_COVER,0);
    lv_obj_set_style_bg_color(obj,color,0);
    return obj;
}
static void set_text(lv_obj_t *obj,const char *value) {
    if(strcmp(lv_label_get_text(obj),value))lv_label_set_text(obj,value);
}
static void init_frames(lv_image_dsc_t *images,const uint8_t *bytes,unsigned count) {
    for(unsigned i=0;i<count;i++)images[i]=(lv_image_dsc_t){
        .header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,
            .w=CAT_WIDTH,.h=CAT_HEIGHT,.stride=CAT_WIDTH*2},
        .data_size=CAT_FRAME_BYTES,.data=bytes+i*CAT_FRAME_BYTES};
}
static void create_ui(void) {
    lv_obj_t *screen=lv_screen_active();
    lv_obj_set_style_bg_color(screen,lv_color_black(),0);
    lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,0);
    lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *ring=lv_obj_create(screen);lv_obj_remove_style_all(ring);
    lv_obj_set_pos(ring,16,16);lv_obj_set_size(ring,434,434);
    lv_obj_set_style_radius(ring,LV_RADIUS_CIRCLE,0);
    lv_obj_set_style_border_width(ring,1,0);lv_obj_set_style_border_color(ring,lv_color_hex(0x26372e),0);
    usb_label=label(83,40,300,&lv_font_montserrat_14,LV_TEXT_ALIGN_CENTER,green());
    lv_label_set_text(usb_label,"USB / WAITING");
    wpm_label=label(173,104,68,&lv_font_montserrat_28,LV_TEXT_ALIGN_RIGHT,lv_color_hex(0xe7ece8));
    lv_label_set_text(wpm_label,"0");
    lv_obj_t *wpm_unit=label(247,115,46,&lv_font_montserrat_14,LV_TEXT_ALIGN_LEFT,muted());
    lv_label_set_text(wpm_unit,"WPM");
    for(unsigned side=0;side<2;side++) {
        int x=side?317:84;
        battery_labels[side]=label(x,86,65,&stopwatch_font_16,LV_TEXT_ALIGN_CENTER,green());
        lv_label_set_text(battery_labels[side],"--%");
        battery_bars[side]=lv_bar_create(screen);
        lv_obj_set_pos(battery_bars[side],x+5,116);lv_obj_set_size(battery_bars[side],55,12);
        if(side)lv_obj_set_style_base_dir(battery_bars[side],LV_BASE_DIR_RTL,0);
        lv_bar_set_range(battery_bars[side],0,100);lv_bar_set_value(battery_bars[side],0,LV_ANIM_OFF);
        lv_obj_set_style_radius(battery_bars[side],6,LV_PART_MAIN);
        lv_obj_set_style_bg_color(battery_bars[side],lv_color_hex(0x142219),LV_PART_MAIN);
        lv_obj_set_style_bg_opa(battery_bars[side],LV_OPA_COVER,LV_PART_MAIN);
        lv_obj_set_style_border_width(battery_bars[side],1,LV_PART_MAIN);
        lv_obj_set_style_border_color(battery_bars[side],lv_color_hex(0x395044),LV_PART_MAIN);
        lv_obj_set_style_radius(battery_bars[side],6,LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(battery_bars[side],green(),LV_PART_INDICATOR);
    }
    init_frames(work_frames,cat_work_start,CAT_WORK_FRAMES);
    init_frames(sleep_frames,cat_sleep_start,CAT_SLEEP_FRAMES);
    cat_image=lv_image_create(screen);lv_obj_set_pos(cat_image,233-CAT_WIDTH/2,257-CAT_HEIGHT/2);
    lv_image_set_src(cat_image,&sleep_frames[0]);
    layer_label=label(43,374,380,&lv_font_montserrat_28,LV_TEXT_ALIGN_CENTER,lv_color_hex(0xf1f4ed));
    lv_label_set_text(layer_label,"BASE");
    message=label(63,412,340,&stopwatch_font_16,LV_TEXT_ALIGN_CENTER,muted());
    lv_label_set_text(message,"");
    for(unsigned i=0;i<5;i++)layer_dots[i]=dot(206+i*12,428,5,i==0?green():lv_color_hex(0x334138));
}
static void update_ui(const struct display_model *m,uint32_t now) {
    set_text(usb_label,m->usb==DISPLAY_USB_READY?"USB / READY":
                       m->usb==DISPLAY_USB_SUSPENDED?"USB / SUSPENDED":"USB / NOT CONNECTED");
    lv_obj_set_style_text_color(usb_label,m->usb==DISPLAY_USB_READY?green():muted(),0);
    bool unknown=false;unsigned ready=0;
    for(unsigned i=0;i<2;i++) {if(m->peers[i].ready)ready++;if(m->peers[i].connected && m->peers[i].side<0)unknown=true;}
    int peers[2]={display_model_find_side(m,0),display_model_find_side(m,1)};
    for(unsigned side=0;side<2;side++) {
        char value[24];lv_color_t color=green();int level=0;
        int p=peers[side];
        if(p<0) {
            snprintf(value,sizeof(value),"%s",unknown?"待识别":"未连接");
            if(!unknown){color=amber();}
        } else if(!m->peers[p].ready)snprintf(value,sizeof(value),"连接中");
        else {

            if(m->peers[p].battery<0)snprintf(value,sizeof(value),"--%%");
            else {level=m->peers[p].battery;snprintf(value,sizeof(value),"%d%%",level);}
            if(m->peers[p].low)color=amber();
        }
        lv_obj_set_style_text_color(battery_labels[side],color,0);
        lv_bar_set_value(battery_bars[side],level,LV_ANIM_OFF);
        lv_obj_set_style_bg_color(battery_bars[side],color,LV_PART_INDICATOR);set_text(battery_labels[side],value);
    }
    const char *names[]={"BASE","NUM / NAV","FN / SYMBOL","SCROLL","SNIPE"};
    char text[80];snprintf(text,sizeof(text),"%u",display_model_wpm(m,now));
    set_text(wpm_label,text);
    snprintf(text,sizeof(text),"%s",m->layer<5?names[m->layer]:"CUSTOM");
    set_text(layer_label,text);
    for(unsigned i=0;i<5;i++)lv_obj_set_style_bg_color(layer_dots[i],i==m->layer?green():lv_color_hex(0x334138),0);
    unsigned pairing=display_model_pairing_seconds(m,now);
    if(m->usb==DISPLAY_USB_SUSPENDED)snprintf(text,sizeof(text),"等待电脑唤醒");
    else if(m->usb!=DISPLAY_USB_READY)snprintf(text,sizeof(text),"请连接电脑 USB");
    else if((pairing && ready<2) || unknown || peers[0]<0 || peers[1]<0 ||
            m->peers[peers[0]].low || m->peers[peers[1]].low) {
        text[0]=0;
    } else if(m->hint!=DISPLAY_HINT_NONE && (uint32_t)(now-m->hint_at)<1200) {
        const char *hints[]={"","音量 +","音量 -","向上滚动","向下滚动"};
        snprintf(text,sizeof(text),"%s",hints[m->hint]);
    } else text[0]=0;
    set_text(message,text);
}
static void display_task(void *arg) {
    uint8_t *buffer=heap_caps_malloc(DISPLAY_BUFFER_BYTES,MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL);
    if(!buffer){ESP_LOGE(TAG,"No LCD DMA buffer; input continues");vTaskDelete(NULL);return;}
    esp_err_t rc=display_hw_init();
    if(rc!=ESP_OK){ESP_LOGE(TAG,"LCD init failed: %s; input continues",esp_err_to_name(rc));free(buffer);vTaskDelete(NULL);return;}
#ifdef CONFIG_STOPWATCH_ORIENTATION
    rotation_canvas=heap_caps_calloc(DISPLAY_WIDTH*DISPLAY_HEIGHT,sizeof(uint16_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    rotation_dma=heap_caps_malloc(DISPLAY_BUFFER_BYTES,MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL);
    if(rotation_canvas && rotation_dma && display_hw_orientation_init()==ESP_OK) {
        rotation_ready=true;imu_ready=true;ESP_LOGI(TAG,"Gravity rotation enabled (PSRAM framebuffer)");
    } else {
        free(rotation_canvas);free(rotation_dma);rotation_canvas=NULL;rotation_dma=NULL;
        ESP_LOGW(TAG,"Rotation unavailable; keeping fixed orientation and keyboard input");
    }
#endif
    lv_init();lv_tick_set_cb(tick_ms);
    lv_display_t *display=lv_display_create(DISPLAY_WIDTH,DISPLAY_HEIGHT);
    if(!display){ESP_LOGE(TAG,"No LVGL display; input continues");free(buffer);vTaskDelete(NULL);return;}
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display,buffer,NULL,DISPLAY_BUFFER_BYTES,LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush);
    lv_display_add_event_cb(display,round_area,LV_EVENT_INVALIDATE_AREA,NULL);
    lv_timer_set_period(lv_display_get_refr_timer(display),10);
    create_ui();
    display_hw_audio_off();
    bool was_working=false,screen_off=false;unsigned last_frame=0,brightness=0;
    uint32_t animation_start=tick_ms(),last_update=animation_start-100;
    while(!display_fault) {
        uint32_t now=tick_ms();
        if((uint32_t)(now-last_update)>=100) {
            last_update=now;struct display_model state;display_status_snapshot(&state);
            unsigned target=display_model_brightness(&state,now);
            if(target==0) {
                if(!screen_off) {
                    rc=display_hw_enabled(false);
                    if(rc!=ESP_OK){ESP_LOGE(TAG,"Screen off failed: %s",esp_err_to_name(rc));display_fault=true;}
                    else screen_off=true;
                }
                // No LVGL timers, animation reads or QSPI pixel transfers while blanked.
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            bool waking=screen_off;
            if(waking){pose.filtered=false;pose.pending=false;}
            sample_orientation(now);
            if(waking) {
                rc=display_hw_brightness(0);
                if(rc!=ESP_OK){display_fault=true;continue;}
                brightness=0;animation_start=now;last_frame=UINT32_MAX;
            }
            update_ui(&state,now);
            bool working=display_model_working(&state,now);
            if(working!=was_working){animation_start=now;last_frame=UINT32_MAX;was_working=working;}
            if(waking) {
                // Refresh the retained framebuffer before exposing it, avoiding stale content.
                lv_obj_invalidate(lv_screen_active());lv_refr_now(display);present_rotated();
                if(display_fault)continue;
                rc=display_hw_enabled(true);
                if(rc!=ESP_OK){ESP_LOGE(TAG,"Screen wake failed: %s",esp_err_to_name(rc));display_fault=true;continue;}
                screen_off=false;
            }
            if(target!=brightness) {
                rc=display_hw_brightness(target);
                if(rc!=ESP_OK){ESP_LOGE(TAG,"Brightness failed: %s",esp_err_to_name(rc));display_fault=true;}
                else brightness=target;
            }
        }
        if(!display_fault && !screen_off){
            const uint16_t *ends=was_working?cat_work_ends:cat_sleep_ends;
            unsigned count=was_working?CAT_WORK_FRAMES:CAT_SLEEP_FRAMES;
            unsigned elapsed=(uint32_t)(now-animation_start)%ends[count-1],frame=0;
            while(frame+1<count && elapsed>=ends[frame])frame++;
            if(frame!=last_frame){
                lv_image_set_src(cat_image,was_working?&work_frames[frame]:&sleep_frames[frame]);
                last_frame=frame;
            }
            lv_timer_handler();present_rotated();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    // A timed-out DMA may still own the buffer: retain resources, stop all redraws.
    // Never restart the MCU, erase bonds, or block the keyboard task on display errors.
    ESP_LOGE(TAG,"Display task disabled until reboot; keyboard remains active");
    vTaskDelete(NULL);
}
#endif
void display_start(void) {
#if CONFIG_STOPWATCH_DISPLAY
    int core=1;
#if CONFIG_FREERTOS_UNICORE
    core=0;
#endif
    if(xTaskCreatePinnedToCore(display_task,"stopwatch_ui",8192,NULL,1,NULL,core)!=pdPASS)
        ESP_LOGE(TAG,"Cannot start display task; input continues");
#endif
}
