// SPDX-License-Identifier: MIT
#include "dongle.h"
#include "ble_output.h"
#include "output_route.h"
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
    struct output_router router={.usb=usb_output,.ble=ble_output_send};
    engine_init(output_router_send,&router);
    keymap_config_init();
    split_start();
    display_start();
    uint8_t states[2][16]={{0}}, merged[16]={0};
    bool mounted=false;
    while (true) {
        struct input_event e;
        bool now_mounted=tud_mounted() && !tud_suspended();
        uint32_t ble_session=ble_output_session();
        display_status_ble_waiting(ble_output_waiting());
        enum output_route next=output_route_select(tud_mounted(),tud_suspended(),ble_session);
        display_status_usb(next==OUTPUT_BLE?DISPLAY_BLE_READY:
                           next==OUTPUT_USB?DISPLAY_USB_READY:
                           tud_suspended()?DISPLAY_USB_SUSPENDED:DISPLAY_USB_OFF);
        display_status_layer(engine_layer());
        if(mounted && !now_mounted)keymap_config_disconnect();
        mounted=now_mounted;
        if (output_router_update(&router,tud_mounted(),tud_suspended(),ble_session,
                                 atomic_exchange(&overflow,false))) {
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
        if (router.route==OUTPUT_OFF) continue;
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
