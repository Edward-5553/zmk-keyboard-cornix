// SPDX-License-Identifier: MIT
#include "keymap_config.h"
#include "keymap_protocol.h"
#include "freertos/FreeRTOS.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs.h"
#include <string.h>
static const char *TAG="keymap_config";
static portMUX_TYPE guard=portMUX_INITIALIZER_UNLOCKED;
static uint8_t incoming[KM_REPORT_SIZE],reply[KM_REPORT_SIZE];
static bool pending,busy;
static struct km_protocol protocol;
static uint32_t get32(const uint8_t *p) {return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put32(uint8_t *p,uint32_t v) {for(unsigned i=0;i<4;i++)p[i]=v>>(i*8);}
static bool save_map(const uint8_t *map,size_t size) {
    uint8_t blob[12+KEYMAP_BYTES]={'S','K','M',1};
    if(size!=KEYMAP_BYTES)return false;
    put32(blob+4,engine_keymap_schema());put32(blob+8,km_hash(map,size));memcpy(blob+12,map,size);
    nvs_handle_t handle;
    esp_err_t err=nvs_open("sw_keymap",NVS_READWRITE,&handle);
    if(err!=ESP_OK){ESP_LOGE(TAG,"NVS open: %s",esp_err_to_name(err));return false;}
    err=nvs_set_blob(handle,"map",blob,sizeof(blob));
    if(err==ESP_OK)err=nvs_commit(handle);
    nvs_close(handle);
    if(err!=ESP_OK)ESP_LOGE(TAG,"Save failed: %s",esp_err_to_name(err));
    return err==ESP_OK;
}
void keymap_config_init(void) {
    bool loaded=false;nvs_handle_t handle;
    esp_err_t err=nvs_open("sw_keymap",NVS_READONLY,&handle);
    if(err==ESP_OK) {
        uint8_t blob[12+KEYMAP_BYTES];size_t size=sizeof(blob);
        err=nvs_get_blob(handle,"map",blob,&size);nvs_close(handle);
        if(err==ESP_OK && size==sizeof(blob) && !memcmp(blob,"SKM\1",4) &&
           get32(blob+4)==engine_keymap_schema() && get32(blob+8)==km_hash(blob+12,KEYMAP_BYTES))
            loaded=engine_keymap_apply(blob+12,KEYMAP_BYTES);
        if(!loaded)ESP_LOGW(TAG,"Saved map incompatible/invalid; using compiled defaults");
    }
    km_protocol_init(&protocol,loaded);
    ESP_LOGI(TAG,"Keymap: %s",loaded?"saved":"compiled defaults");
}
void keymap_config_receive(const uint8_t *data,size_t size) {
    if(size!=KM_REPORT_SIZE)return;
    portENTER_CRITICAL(&guard);
    if(!busy){memcpy(incoming,data,size);busy=pending=true;}
    portEXIT_CRITICAL(&guard);
}
size_t keymap_config_report(uint8_t *out,size_t size) {
    if(size>KM_REPORT_SIZE)size=KM_REPORT_SIZE;
    portENTER_CRITICAL(&guard);memcpy(out,reply,size);portEXIT_CRITICAL(&guard);
    return size;
}
bool keymap_config_poll(void) {
    uint8_t request[KM_REPORT_SIZE],response[KM_REPORT_SIZE];bool ready;
    portENTER_CRITICAL(&guard);ready=pending;
    if(ready){memcpy(request,incoming,sizeof(request));pending=false;}
    portEXIT_CRITICAL(&guard);
    if(!ready)return false;
    bool changed=km_protocol_request(&protocol,request,response,esp_timer_get_time()/1000,save_map);
    portENTER_CRITICAL(&guard);memcpy(reply,response,sizeof(reply));busy=false;portEXIT_CRITICAL(&guard);
    return changed;
}
void keymap_config_disconnect(void) {
    km_protocol_disconnect(&protocol);
    portENTER_CRITICAL(&guard);pending=busy=false;memset(reply,0,sizeof(reply));portEXIT_CRITICAL(&guard);
}
