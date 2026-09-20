// SPDX-License-Identifier: MIT
#include "display_model.h"
#include <string.h>

void display_model_init(struct display_model *m) {
    memset(m, 0, sizeof(*m));
    for (unsigned i=0; i<2; i++) m->peers[i].side=m->peers[i].battery=-1;
}
void display_model_connect(struct display_model *m, unsigned peer, const uint8_t identity[7]) {
    if (peer>=2) return;
    struct display_peer *p=&m->peers[peer];
    *p=(struct display_peer){.connected=true,.side=-1,.battery=-1};
    memcpy(p->identity,identity,7);
    for (unsigned side=0; side<2; side++)
        if (m->known[side] && !memcmp(identity,m->known_identity[side],7)) p->side=side;
}
void display_model_disconnect(struct display_model *m, unsigned peer) {
    if (peer>=2) return;
    m->peers[peer].connected=m->peers[peer].ready=m->peers[peer].low=false;
    m->peers[peer].battery=-1;
}
void display_model_side(struct display_model *m, unsigned peer, unsigned side) {
    if (peer>=2 || side>=2 || !m->peers[peer].connected) return;
    // Never steal a side already claimed by another live peer.
    if (m->peers[1-peer].connected && m->peers[1-peer].side==(int)side) return;
    struct display_peer *p=&m->peers[peer];
    if (p->side>=0 && p->side!=(int)side) return;
    p->side=side;
    memcpy(m->known_identity[side],p->identity,7);
    m->known[side]=true;
}
void display_model_positions(struct display_model *m, unsigned peer, const uint8_t bits[16]) {
    unsigned sides=0;
    for (unsigned pos=0;pos<50;pos++) if (bits[pos/8] & (1u<<(pos%8))) {
        bool left=pos<6 || (pos>=12 && pos<18) || (pos>=24 && pos<=30) || (pos>=38 && pos<44);
        sides|=left?1:2;
    }
    if (sides==1 || sides==2) display_model_side(m,peer,sides==1?0:1);
}
void display_model_battery(struct display_model *m, unsigned peer, int level) {
    if (peer>=2 || !m->peers[peer].connected) return;
    struct display_peer *p=&m->peers[peer];
    p->battery=level>=0 && level<=100 ? level : -1;
    if (p->battery<0 || p->battery>=25) p->low=false;
    else if (p->battery<20) p->low=true;
}
int display_model_find_side(const struct display_model *m, unsigned side) {
    for (int i=0;i<2;i++) if (m->peers[i].connected && m->peers[i].side==(int)side) return i;
    return -1;
}
bool display_model_working(const struct display_model *m, uint32_t now) {
    return m->usb==DISPLAY_USB_READY && m->has_activity && (uint32_t)(now-m->last_activity)<3000;
}
unsigned display_model_brightness(const struct display_model *m, uint32_t now) {
    uint32_t idle=now-m->last_activity;
    if (m->usb==DISPLAY_USB_SUSPENDED || idle>=DISPLAY_OFF_MS) return 0;
    if (m->usb!=DISPLAY_USB_READY) return 5;
    return idle>=DISPLAY_DIM_MS ? 20 : 35;
}
unsigned display_model_pairing_seconds(const struct display_model *m, uint32_t now) {
    int32_t remaining=(int32_t)(m->pairing_until-now);
    return m->pairing_until && remaining>0 ? ((uint32_t)remaining+999)/1000 : 0;
}

// Approximate text speed from new printable HID usages, not physical layer keys.
void display_model_wpm_reset(struct display_model *m) {
    memset(m->wpm_keys,0,sizeof(m->wpm_keys));
    memset(m->wpm_counts,0,sizeof(m->wpm_counts));m->wpm_active=false;
}
void display_model_keyboard(struct display_model *m,uint8_t mods,const uint8_t keys[6],uint32_t now) {
    unsigned count=0;
    for(unsigned i=0;i<6;i++) {
        unsigned k=keys[i];bool existing=false;
        for(unsigned j=0;j<6;j++)if(m->wpm_keys[j]==k)existing=true;
        for(unsigned j=0;j<i;j++)if(keys[j]==k)existing=true;
        bool printable=(k>=4 && k<=39)||(k>=44 && k<=56)||(k>=84 && k<=87)||(k>=89 && k<=100)||k==103;
        if(!existing && printable && !(mods&0xdd))count++; // Shift allowed; Ctrl/Alt/GUI excluded.
    }
    memcpy(m->wpm_keys,keys,6);
    if(m->usb!=DISPLAY_USB_READY || !count)return;
    if(!m->wpm_active || (uint32_t)(now-m->wpm_last)>=3000) {
        memset(m->wpm_counts,0,sizeof(m->wpm_counts));m->wpm_slot=0;m->wpm_epoch=now;m->wpm_active=true;
    }
    unsigned steps=(uint32_t)(now-m->wpm_epoch)/1000;
    if(steps>=10){memset(m->wpm_counts,0,sizeof(m->wpm_counts));m->wpm_slot=0;m->wpm_epoch=now;}
    else while(steps--){m->wpm_epoch+=1000;m->wpm_slot=(m->wpm_slot+1)%10;m->wpm_counts[m->wpm_slot]=0;}
    unsigned value=m->wpm_counts[m->wpm_slot]+count;
    m->wpm_counts[m->wpm_slot]=value>UINT16_MAX?UINT16_MAX:value;m->wpm_last=now;
}
unsigned display_model_wpm(const struct display_model *m,uint32_t now) {
    if(m->usb!=DISPLAY_USB_READY || !m->wpm_active || (uint32_t)(now-m->wpm_last)>=3000)return 0;
    unsigned elapsed=(uint32_t)(now-m->wpm_epoch)/1000,total=0;
    for(unsigned age=0;age<10 && age+elapsed<10;age++)total+=m->wpm_counts[(m->wpm_slot+10-age)%10];
    unsigned result=(total*6+2)/5; // 10 seconds -> per minute, then five characters per word.
    return result>999?999:result;
}

void display_model_usb(struct display_model *m,enum display_usb usb,uint32_t now) {
    if(m->usb==usb)return;
    display_model_wpm_reset(m);
    // USB re-enumeration/resume wakes the display without pretending to type.
    if(usb==DISPLAY_USB_READY){m->last_activity=now;m->has_activity=false;}
    m->usb=usb;
}
