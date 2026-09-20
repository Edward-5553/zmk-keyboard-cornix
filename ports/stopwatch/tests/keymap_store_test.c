// SPDX-License-Identifier: MIT
// Runs the real mailbox and persistence code against an in-memory NVS implementation.
#include "keymap_config.h"
#include "keymap_protocol.h"
#include "nvs.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static uint8_t disk[KEYMAP_BYTES+12],pending_disk[sizeof(disk)];
static bool exists,fail_commit;
static uint8_t q[63],r[63],map[KEYMAP_BYTES],original[KEYMAP_BYTES];
static uint32_t seq=10,token;
int64_t esp_timer_get_time(void){return 1000000;}
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *h){assert(!strcmp(name,"sw_keymap"));*h=1;return mode==NVS_READONLY&&!exists?1:0;}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *data,size_t *size){assert(!strcmp(key,"map"));assert(*size==sizeof(disk));memcpy(data,disk,*size);return 0;}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t size){assert(!strcmp(key,"map")&&size==sizeof(disk));memcpy(pending_disk,data,size);return 0;}
esp_err_t nvs_commit(nvs_handle_t h){if(fail_commit)return 1;memcpy(disk,pending_disk,sizeof(disk));exists=true;return 0;}
void nvs_close(nvs_handle_t h){}
static void output(const struct output *o,void *ctx){}
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=v>>(i*8);}
static void command(unsigned op){memset(q,0,sizeof(q));q[0]='S';q[1]='K';q[2]=1;q[3]=op;put32(q+4,++seq);}
static void send(unsigned expected){keymap_config_receive(q,sizeof(q));keymap_config_poll();assert(keymap_config_report(r,sizeof(r))==63);assert(r[8]==expected&&!memcmp(q+4,r+4,4));}
static void upload(void){
    command(KM_BEGIN);put32(q+8,1);put32(q+12,engine_keymap_schema());token=seq;send(KM_OK);
    for(unsigned o=0;o<KEYMAP_BYTES;o+=48){unsigned n=KEYMAP_BYTES-o;if(n>48)n=48;
        command(KM_WRITE);put32(q+8,token);q[12]=o;q[13]=o>>8;q[14]=n;memcpy(q+15,map+o,n);send(KM_OK);}
}
static void commit(unsigned result){command(KM_COMMIT);put32(q+8,token);put32(q+12,km_hash(map,sizeof(map)));send(result);}
static void reboot(void){keymap_config_disconnect();engine_init(output,NULL);keymap_config_init();}
int main(void){
    reboot();engine_keymap_export(original,false);memcpy(map,original,sizeof(map));map[8]=4;
    upload();fail_commit=true;commit(KM_STORAGE);assert(!exists);
    uint8_t current[KEYMAP_BYTES];engine_keymap_export(current,false);assert(!memcmp(current,original,sizeof(current)));
    fail_commit=false;commit(KM_OK);assert(exists);
    reboot();engine_keymap_export(current,false);assert(!memcmp(current,map,sizeof(current)));
    command(KM_INFO);send(KM_OK);assert(r[21]==1);
    // Corruption or incompatible schema never applies a partially valid saved map.
    disk[12]^=1;reboot();engine_keymap_export(current,false);assert(!memcmp(current,original,sizeof(current)));
    disk[12]^=1;disk[4]^=1;reboot();engine_keymap_export(current,false);assert(!memcmp(current,original,sizeof(current)));
    puts("NVS integration: commit failure, successful reload, checksum/schema rejection passed");
}
