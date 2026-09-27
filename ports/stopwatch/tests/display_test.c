// SPDX-License-Identifier: MIT
#include "display_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    struct display_model m;
    const uint8_t left[7]={1,2},right[7]={1,3},other[7]={1,4};
    uint8_t bits[16]={0};
    display_model_init(&m);
    assert(m.peers[0].battery==-1 && display_model_find_side(&m,0)==-1);
    display_model_connect(&m,0,right);display_model_connect(&m,1,left);
    display_model_positions(&m,0,bits);assert(m.peers[0].side==-1);
    bits[0]=0x41;display_model_positions(&m,0,bits);assert(m.peers[0].side==-1);
    bits[0]=0x40;display_model_positions(&m,0,bits);
    bits[0]=1;display_model_positions(&m,1,bits);
    assert(display_model_find_side(&m,0)==1 && display_model_find_side(&m,1)==0);
    display_model_side(&m,0,0);assert(m.peers[0].side==1);
    display_model_battery(&m,0,0);assert(m.peers[0].battery==0 && m.peers[0].low);
    display_model_battery(&m,0,24);assert(m.peers[0].low);
    display_model_battery(&m,0,25);assert(!m.peers[0].low);
    display_model_battery(&m,0,20);assert(!m.peers[0].low);
    display_model_battery(&m,0,19);assert(m.peers[0].low);
    display_model_battery(&m,0,101);assert(m.peers[0].battery==-1 && !m.peers[0].low);
    m.peers[0].ready=true;display_model_disconnect(&m,0);
    assert(!m.peers[0].ready && display_model_find_side(&m,1)==-1 && m.peers[0].battery==-1);
    display_model_disconnect(&m,1);
    display_model_connect(&m,1,right);assert(m.peers[1].side==1);
    display_model_connect(&m,0,other);assert(m.peers[0].side==-1);
    display_model_side(&m,0,0);assert(m.peers[0].side==0);
    // Exercise all 50 physical positions independently, including bottom keys.
    for(unsigned pos=0;pos<50;pos++) {
        display_model_init(&m);display_model_connect(&m,0,left);
        memset(bits,0,sizeof(bits));bits[pos/8]=1u<<(pos%8);
        display_model_positions(&m,0,bits);
        unsigned side=pos<6 || (pos>=12 && pos<18) || (pos>=24 && pos<31) || (pos>=38 && pos<44)?0:1;
        assert(m.peers[0].side==(int)side);
    }
    display_model_init(&m);m.usb=DISPLAY_USB_READY;
    assert(!display_model_working(&m,0) && display_model_brightness(&m,59999)==35);
    assert(display_model_brightness(&m,60000)==20);
    m.has_activity=true;m.last_activity=UINT32_MAX-100;
    assert(display_model_working(&m,2898) && !display_model_working(&m,2899));
    assert(display_model_brightness(&m,59899)==20);
    m.usb=DISPLAY_USB_SUSPENDED;
    assert(!display_model_working(&m,0) && display_model_brightness(&m,0)==0);
    m.pairing_until=1000;
    assert(display_model_pairing_seconds(&m,UINT32_MAX-999)==2);
    assert(display_model_pairing_seconds(&m,999)==1 && display_model_pairing_seconds(&m,1000)==0);
    m.pairing_until=0;assert(display_model_pairing_seconds(&m,UINT32_MAX)==0);
    display_model_init(&m);m.usb=DISPLAY_USB_READY;
    assert(display_model_brightness(&m,119999)==20);
    assert(display_model_brightness(&m,120000)==0);
    // Activity wakes immediately; reconnect/resume also gives a fresh idle interval.
    m.last_activity=120010;m.has_activity=true;
    assert(display_model_brightness(&m,120010)==35);
    display_model_usb(&m,DISPLAY_USB_SUSPENDED,120020);
    assert(display_model_brightness(&m,120020)==0);
    display_model_usb(&m,DISPLAY_USB_READY,500000);
    assert(display_model_brightness(&m,500000)==35 && !display_model_working(&m,500000));
    display_model_usb(&m,DISPLAY_USB_READY,620000); // repeated polls must not reset timeout
    assert(display_model_brightness(&m,620000)==0);
    m.last_activity=UINT32_MAX-100;
    assert(display_model_brightness(&m,119898)==20);
    assert(display_model_brightness(&m,119899)==0);
    m.usb=DISPLAY_USB_OFF;assert(display_model_brightness(&m,119899)==0);
    // 50 printable key presses in ten seconds = 60 WPM; reports/releases do not double count.
    display_model_init(&m);m.usb=DISPLAY_USB_READY;
    uint8_t key[6]={4},release[6]={0};
    for(unsigned i=0;i<50;i++) {
        display_model_keyboard(&m,0,key,i*200);
        display_model_keyboard(&m,0,key,i*200+1);
        display_model_keyboard(&m,0,release,i*200+2);
    }
    assert(display_model_wpm(&m,9802)==60);
    assert(display_model_wpm(&m,10800)==54); // oldest second expires
    assert(display_model_wpm(&m,12800)==0); // idle reset
    display_model_wpm_reset(&m);
    display_model_keyboard(&m,1,key,20000); // Ctrl+A shortcut
    display_model_keyboard(&m,0,release,20001);
    key[0]=80;display_model_keyboard(&m,0,key,20002); // arrow
    assert(display_model_wpm(&m,20002)==0);
    key[0]=4;display_model_keyboard(&m,2,key,20100); // Shift+A counts
    assert(display_model_wpm(&m,20100)==1);
    display_model_wpm_reset(&m);
    display_model_keyboard(&m,0,key,UINT32_MAX-500);
    display_model_keyboard(&m,0,release,UINT32_MAX-400);
    display_model_keyboard(&m,0,key,600);
    assert(display_model_wpm(&m,600)==2);
    m.usb=DISPLAY_USB_SUSPENDED;assert(display_model_wpm(&m,600)==0);
    display_model_usb(&m,DISPLAY_BLE_READY,1000000);
    assert(display_model_brightness(&m,1000000)==35);
    display_model_keyboard(&m,0,key,1000001);
    assert(display_model_wpm(&m,1000001)==1);
    m.has_activity=true;m.last_activity=1000001;
    assert(display_model_working(&m,1000002));
    assert(display_model_brightness(&m,1060001)==20);
    assert(display_model_brightness(&m,1120001)==0);
    display_model_usb(&m,DISPLAY_USB_READY,1200000);
    assert(display_model_wpm(&m,1200000)==0 && display_model_brightness(&m,1200000)==35);
    // Actual output wins over Bluetooth pairing/reconnect activity.
    display_model_init(&m);assert(display_model_connection_icon(&m)==DISPLAY_ICON_OFF);
    m.ble_waiting=true;assert(display_model_connection_icon(&m)==DISPLAY_ICON_WAITING);
    m.usb=DISPLAY_USB_READY;assert(display_model_connection_icon(&m)==DISPLAY_ICON_USB);
    m.usb=DISPLAY_BLE_READY;assert(display_model_connection_icon(&m)==DISPLAY_ICON_BLE);
    m.usb=DISPLAY_USB_SUSPENDED;assert(display_model_connection_icon(&m)==DISPLAY_ICON_OFF);
    m.usb=DISPLAY_USB_OFF;m.ble_waiting=false;assert(display_model_connection_icon(&m)==DISPLAY_ICON_OFF);
    puts("Display identity, battery, disconnect, activity and timer tests passed");
}
