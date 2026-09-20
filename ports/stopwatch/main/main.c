// SPDX-License-Identifier: MIT
#include "dongle.h"
#include "display_status.h"
#include "keymap_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "tusb.h"
#include <stdatomic.h>
#include <string.h>

static QueueHandle_t inputs;
static atomic_bool overflow;
static uint32_t millis(void) { return esp_timer_get_time()/1000; }
void dongle_input(const struct input_event *event) {
    if (xQueueSend(inputs,event,0)!=pdTRUE) atomic_store(&overflow,true);
}

void app_main(void) {
    // Never silently erase bonds on an NVS error; recovery is an explicit flash operation.
    ESP_ERROR_CHECK(nvs_flash_init());
    inputs=xQueueCreate(256,sizeof(struct input_event));
    configASSERT(inputs);
    display_status_init();
    usb_start();
    engine_init(usb_output,NULL);
    keymap_config_init();
    split_start();
    display_start();
    uint8_t states[2][16]={{0}}, merged[16]={0};
    bool mounted=false;
    while (true) {
        struct input_event e;
        bool now_mounted=tud_mounted() && !tud_suspended();
        display_status_usb(tud_suspended()?DISPLAY_USB_SUSPENDED:
                           now_mounted?DISPLAY_USB_READY:DISPLAY_USB_OFF);
        display_status_layer(engine_layer());
        if (now_mounted!=mounted || atomic_exchange(&overflow,false)) {
            if(!now_mounted)keymap_config_disconnect();
            mounted=now_mounted;
            engine_cancel();
            memset(states,0,sizeof(states));
            memset(merged,0,sizeof(merged));
            xQueueReset(inputs);
        }
        usb_poll();
        if(keymap_config_poll()) {
            memset(states,0,sizeof(states));memset(merged,0,sizeof(merged));xQueueReset(inputs);
        }
        if (xQueueReceive(inputs,&e,pdMS_TO_TICKS(5))!=pdTRUE) {
            engine_tick(millis());
            continue;
        }
        if (!mounted) continue;
        if (e.peer>=2) continue;
        if (e.kind==INPUT_DISCONNECT) {
            // Clear both sources to avoid a held layer/queued macro leaving stuck keys.
            engine_cancel();
            memset(states,0,sizeof(states));
            memset(merged,0,sizeof(merged));
            continue;
        }
        if (e.kind==INPUT_SENSOR) { engine_sensor(e.bytes,e.len); continue; }
        if (e.len!=16) continue;
        memcpy(states[e.peer],e.bytes,16);
        for (unsigned pos=0; pos<50; pos++) {
            uint8_t mask=1u<<(pos%8), byte=pos/8;
            bool pressed=(states[0][byte]|states[1][byte])&mask;
            if (pressed!=!!(merged[byte]&mask)) {
                if (pressed) merged[byte]|=mask; else merged[byte]&=~mask;
                engine_position(pos,pressed,e.at);
                if(pressed)display_status_activity(DISPLAY_HINT_NONE);
            }
        }
    }
}
