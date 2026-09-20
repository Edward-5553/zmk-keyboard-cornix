// SPDX-License-Identifier: MIT
#include "keymap_protocol.h"
#include <string.h>
static uint32_t get32(const uint8_t *p) {return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put32(uint8_t *p,uint32_t v) {for(unsigned i=0;i<4;i++)p[i]=v>>(i*8);}
uint32_t km_hash(const uint8_t *data,size_t size) {
    uint32_t hash=2166136261u;
    for(size_t i=0;i<size;i++)hash=(hash^data[i])*16777619u;
    return hash;
}
void km_protocol_init(struct km_protocol *p,bool saved) {
    memset(p,0,sizeof(*p));p->revision=1;p->saved=saved;
}
void km_protocol_disconnect(struct km_protocol *p) {p->staging_active=false;p->received=0;}
bool km_protocol_request(struct km_protocol *p,const uint8_t q[KM_REPORT_SIZE],uint8_t r[KM_REPORT_SIZE],uint32_t now,km_save_fn save) {
    memset(r,0,KM_REPORT_SIZE);memcpy(r,q,8);r[0]='S';r[1]='K';r[2]=1;
    if(q[0]!='S' || q[1]!='K' || q[2]!=1){r[8]=KM_BAD_REQUEST;return false;}
    if(p->staging_active && (uint32_t)(now-p->touched)>30000)km_protocol_disconnect(p);
    const uint8_t *a=q+8;uint8_t *b=r+9;
    switch(q[3]) {
    case KM_INFO:
        b[0]=KEYMAP_LAYERS;b[1]=KEYMAP_KEYS;b[2]=6;b[3]=KM_CHUNK;
        put32(b+4,engine_keymap_schema());put32(b+8,p->revision);b[12]=p->saved;break;
    case KM_READ: {
        unsigned offset=a[0]|(unsigned)a[1]<<8,count=a[2];
        if(!count || count>KM_CHUNK || offset+count>KEYMAP_BYTES || a[3]>1){r[8]=KM_BAD_REQUEST;break;}
        uint8_t map[KEYMAP_BYTES];engine_keymap_export(map,a[3]==1);
        b[0]=a[0];b[1]=a[1];b[2]=count;memcpy(b+3,map+offset,count);break;
    }
    case KM_BEGIN:
        if(get32(a+4)!=engine_keymap_schema()){r[8]=KM_SCHEMA;break;}
        if(get32(a)!=p->revision){r[8]=KM_CONFLICT;break;}
        if(p->staging_active){r[8]=KM_BUSY;break;}
        p->token=get32(q+4);p->staging_active=true;p->received=0;p->touched=now;
        put32(b,p->token);break;
    case KM_WRITE: {
        if(!p->staging_active || get32(a)!=p->token){r[8]=KM_EXPIRED;break;}
        unsigned offset=a[4]|(unsigned)a[5]<<8,count=a[6];
        if(!count || count>KM_CHUNK || offset+count>KEYMAP_BYTES){r[8]=KM_BAD_REQUEST;break;}
        if(offset!=p->received){r[8]=KM_ORDER;break;}
        memcpy(p->staging+offset,a+7,count);p->received+=count;p->touched=now;break;
    }
    case KM_COMMIT:
        if(!p->staging_active || get32(a)!=p->token){r[8]=KM_EXPIRED;break;}
        if(p->received!=KEYMAP_BYTES || get32(a+4)!=km_hash(p->staging,KEYMAP_BYTES) ||
           !engine_keymap_validate(p->staging,KEYMAP_BYTES)){r[8]=KM_INVALID;break;}
        if(!engine_idle()){r[8]=KM_BUSY;break;}
        if(!save(p->staging,KEYMAP_BYTES)){r[8]=KM_STORAGE;break;}
        engine_keymap_apply(p->staging,KEYMAP_BYTES);
        p->revision++;p->saved=true;km_protocol_disconnect(p);put32(b,p->revision);return true;
    case KM_ABORT:
        if(p->staging_active && get32(a)!=p->token){r[8]=KM_CONFLICT;break;}
        km_protocol_disconnect(p);break;
    default:r[8]=KM_BAD_REQUEST;
    }
    return false;
}
