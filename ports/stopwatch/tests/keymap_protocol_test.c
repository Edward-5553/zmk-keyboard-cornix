// SPDX-License-Identifier: MIT
#include "keymap_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct km_protocol protocol;
static uint8_t request[KM_REPORT_SIZE],response[KM_REPORT_SIZE],original[KEYMAP_BYTES],edited[KEYMAP_BYTES],actual[KEYMAP_BYTES];
static unsigned saves;static bool storage_ok=true;static uint32_t now,seq=100;
static uint8_t last_key;
static void output(const struct output *o,void *ctx) {if(o->kind==OUT_KEYBOARD)last_key=o->keys[0];}
static bool save(const uint8_t *p,size_t size) {assert(size==KEYMAP_BYTES);saves++;return storage_ok;}
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=v>>(8*i);}
static void command(unsigned cmd) {memset(request,0,sizeof(request));request[0]='S';request[1]='K';request[2]=1;request[3]=cmd;put32(request+4,++seq);}
static bool run(unsigned expected) {bool changed=km_protocol_request(&protocol,request,response,now,save);assert(response[8]==expected);return changed;}
static void unchanged(void) {engine_keymap_export(actual,false);assert(!memcmp(actual,original,sizeof(actual)));}
static void begin(void) {command(KM_BEGIN);put32(request+8,protocol.revision);put32(request+12,engine_keymap_schema());run(KM_OK);}
static void upload(void) {
    for(unsigned o=0;o<KEYMAP_BYTES;o+=KM_CHUNK) {
        unsigned n=KEYMAP_BYTES-o;if(n>KM_CHUNK)n=KM_CHUNK;
        command(KM_WRITE);put32(request+8,protocol.token);request[12]=o;request[13]=o>>8;request[14]=n;
        memcpy(request+15,edited+o,n);run(KM_OK);
    }
}
static void commit(unsigned expected) {command(KM_COMMIT);put32(request+8,protocol.token);put32(request+12,km_hash(edited,sizeof(edited)));run(expected);}
int main(void) {
    engine_init(output,NULL);km_protocol_init(&protocol,false);engine_keymap_export(original,false);
    memcpy(edited,original,sizeof(edited));edited[6+2]=4; // Q -> A
    command(KM_BEGIN);put32(request+8,1);put32(request+12,0);run(KM_SCHEMA);unchanged();
    command(KM_BEGIN);put32(request+8,2);put32(request+12,engine_keymap_schema());run(KM_CONFLICT);
    begin();command(KM_WRITE);put32(request+8,protocol.token);request[12]=48;request[14]=48;run(KM_ORDER);unchanged();
    commit(KM_INVALID);assert(saves==0);unchanged();
    km_protocol_disconnect(&protocol);commit(KM_EXPIRED);unchanged();
    begin();upload();unchanged();
    engine_position(1,true,1);assert(last_key==20);commit(KM_BUSY);assert(saves==0);
    engine_position(1,false,2);storage_ok=false;commit(KM_STORAGE);unchanged();
    storage_ok=true;commit(KM_OK);assert(protocol.revision==2 && protocol.saved);
    engine_position(1,true,3);assert(last_key==4);engine_position(1,false,4);
    engine_keymap_export(actual,true);assert(!memcmp(actual,original,sizeof(actual)));
    // A malformed later binding cannot partially change the map.
    edited[1494]=255;assert(!engine_keymap_apply(edited,sizeof(edited)));
    engine_keymap_export(actual,false);assert(actual[8]==4);
    edited[1494]=original[1494];edited[0]=B_LT;edited[4]=7;edited[5]=255;
    assert(!engine_keymap_validate(edited,sizeof(edited)));
    memcpy(edited,original,sizeof(edited));edited[0]=B_MACRO;edited[2]=255;
    assert(!engine_keymap_validate(edited,sizeof(edited)));
    // Expiration and stale transaction tokens do not commit partial maps.
    now=UINT32_MAX-100;begin();now=30000;commit(KM_EXPIRED);
    now=0;begin();uint32_t token=protocol.token;
    command(KM_ABORT);put32(request+8,token+1);run(KM_CONFLICT);assert(protocol.staging_active);
    command(KM_ABORT);put32(request+8,token);run(KM_OK);assert(!protocol.staging_active);
    command(KM_READ);request[8]=255;request[9]=255;request[10]=48;run(KM_BAD_REQUEST);
    command(KM_INFO);request[2]=2;run(KM_BAD_REQUEST);
    puts("Keymap transactions: partial/invalid/stale writes, held keys, storage failure, save/apply, expiry passed");
}
