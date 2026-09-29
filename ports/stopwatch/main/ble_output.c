// SPDX-License-Identifier: MIT
// One bonded computer, separate from the two ZMK central connections.
#include "ble_output.h"
#include "output_route.h"
#include "display_status.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nimble/nimble_port.h"
#include "host/ble_store.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "nvs.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "tusb.h"
#include <stdatomic.h>
#include <string.h>

static const char *TAG="ble_output";
static const uint8_t report_map[]={
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(1)),
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(2)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(3)),
};
static uint16_t handles[3],conn=BLE_HS_CONN_HANDLE_NONE;
static bool subscribed[3],encrypted,suspended,closing;
static atomic_bool host_saved;
static atomic_uint reset_result;
static ble_addr_t host_identity;
static nvs_handle_t storage;
static uint8_t own_type;
static uint32_t generation;
static int64_t pairing_until,blocked_since;
static atomic_uint session,report_epoch;
static atomic_bool waiting;
static atomic_bool forget_requested,queue_failed;
static struct ble_npl_callout timer;
struct queued_report { struct output output; uint32_t session,epoch; };
static QueueHandle_t reports;
static uint8_t values[3][8];
static const uint8_t lengths[]={8,2,5};
static int gap(struct ble_gap_event *event,void *arg);

bool ble_output_is_host(const ble_addr_t *address) {
    return host_saved && !ble_addr_cmp(address,&host_identity);
}
uint32_t ble_output_session(void) { return atomic_load(&session); }
bool ble_output_waiting(void) { return atomic_load(&waiting); }
void ble_output_forget(void) { atomic_store(&reset_result,1);atomic_store(&forget_requested,true); }
void ble_output_status(uint8_t status[8]) {
    memcpy(status,"SWBT",4);status[4]=1;status[5]=atomic_load(&host_saved);
    status[6]=ble_output_session()!=0;status[7]=atomic_load(&reset_result);
}

static void readiness(void) {
    bool ready=conn!=BLE_HS_CONN_HANDLE_NONE && encrypted && host_saved && subscribed[0] && !suspended && !closing;
    if(ready && !atomic_load(&session)) {
        if(!++generation)++generation;
        atomic_store(&session,generation);
    } else if(!ready)atomic_store(&session,0);
}
static void reset_link(void) {
    atomic_store(&session,0);
    encrypted=suspended=closing=false;
    memset(subscribed,0,sizeof(subscribed));
    memset(values,0,sizeof(values));
    xQueueReset(reports);
    atomic_store(&queue_failed,false);
    blocked_since=0;
}
static int append(struct os_mbuf *om,const void *data,size_t len) {
    return os_mbuf_append(om,data,len)?BLE_ATT_ERR_INSUFFICIENT_RES:0;
}
static int access(uint16_t connection,uint16_t handle,struct ble_gatt_access_ctxt *ctxt,void *arg) {
    unsigned item=(uintptr_t)arg;
    if(ctxt->op==BLE_GATT_ACCESS_OP_READ_DSC) {
        const uint8_t ref[]={(uint8_t)(item==3?1:item+1),(uint8_t)(item==3?2:1)};
        return append(ctxt->om,ref,sizeof(ref));
    }
    if(ctxt->op==BLE_GATT_ACCESS_OP_READ_CHR) {
        const uint8_t info[]={0x11,0x01,0,0x02}; // HID 1.11, normally connectable, no remote wake.
        if(item<3)return append(ctxt->om,values[item],lengths[item]);
        if(item==3){uint8_t leds=0;return append(ctxt->om,&leds,1);}
        if(item==4)return append(ctxt->om,report_map,sizeof(report_map));
        if(item==5)return append(ctxt->om,info,sizeof(info));
        // Development USB vendor/product identity; no allocated commercial VID/PID.
        if(item==8){const uint8_t pnp[]={2,0xfe,0xca,0x23,0x40,0,1};return append(ctxt->om,pnp,7);}
    }
    if(ctxt->op==BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint8_t byte;
        if(OS_MBUF_PKTLEN(ctxt->om)!=1 || os_mbuf_copydata(ctxt->om,0,1,&byte))
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        if(item==3)return 0; // Accept host LEDs; no split LED forwarding in this version.
        if(item==7 && byte<=1){suspended=byte==0;readiness();return 0;}
    }
    return BLE_ATT_ERR_UNLIKELY;
}
#define REF(n) ((struct ble_gatt_dsc_def[]){{.uuid=BLE_UUID16_DECLARE(0x2908), \
    .att_flags=BLE_ATT_F_READ,.access_cb=access,.arg=(void *)(uintptr_t)(n)},{0}})
#define INPUT(n) {.uuid=BLE_UUID16_DECLARE(0x2a4d),.access_cb=access,.arg=(void *)(uintptr_t)(n), \
    .flags=BLE_GATT_CHR_F_READ|BLE_GATT_CHR_F_READ_ENC|BLE_GATT_CHR_F_NOTIFY, \
    .val_handle=&handles[n],.descriptors=REF(n)}
static const struct ble_gatt_svc_def services[]={
    {.type=BLE_GATT_SVC_TYPE_PRIMARY,.uuid=BLE_UUID16_DECLARE(0x1812),
     .characteristics=(struct ble_gatt_chr_def[]){
        INPUT(0),INPUT(1),INPUT(2),
        {.uuid=BLE_UUID16_DECLARE(0x2a4d),.access_cb=access,.arg=(void *)3,
         .flags=BLE_GATT_CHR_F_READ|BLE_GATT_CHR_F_WRITE|BLE_GATT_CHR_F_WRITE_NO_RSP|
                BLE_GATT_CHR_F_READ_ENC|BLE_GATT_CHR_F_WRITE_ENC,.descriptors=REF(3)},
        {.uuid=BLE_UUID16_DECLARE(0x2a4b),.access_cb=access,.arg=(void *)4,.flags=BLE_GATT_CHR_F_READ},
        {.uuid=BLE_UUID16_DECLARE(0x2a4a),.access_cb=access,.arg=(void *)5,.flags=BLE_GATT_CHR_F_READ},
        {.uuid=BLE_UUID16_DECLARE(0x2a4c),.access_cb=access,.arg=(void *)7,
         .flags=BLE_GATT_CHR_F_WRITE_NO_RSP|BLE_GATT_CHR_F_WRITE_ENC},{0}}},
    {.type=BLE_GATT_SVC_TYPE_PRIMARY,.uuid=BLE_UUID16_DECLARE(0x180a),
     .characteristics=(struct ble_gatt_chr_def[]){
        {.uuid=BLE_UUID16_DECLARE(0x2a50),.access_cb=access,.arg=(void *)8,.flags=BLE_GATT_CHR_F_READ},{0}}},
    {0}};

static void advertise(void) {
    if(conn!=BLE_HS_CONN_HANDLE_NONE || ble_gap_adv_active())return;
    if(!host_saved && esp_timer_get_time()>=pairing_until)return;
    const ble_uuid16_t hid=BLE_UUID16_INIT(0x1812);
    struct ble_hs_adv_fields fields={
        .flags=BLE_HS_ADV_F_DISC_GEN|BLE_HS_ADV_F_BREDR_UNSUP,
        .uuids16=&hid,.num_uuids16=1,.uuids16_is_complete=1,
        .appearance=0x03c1,.appearance_is_present=1,
    };
    int rc=ble_gap_adv_set_fields(&fields);
    if(!rc){
        struct ble_hs_adv_fields response={.name=(const uint8_t *)"Cornix StopWatch BLE",
            .name_len=19,.name_is_complete=1};
        rc=ble_gap_adv_rsp_set_fields(&response);
    }
    struct ble_gap_adv_params params={.conn_mode=BLE_GAP_CONN_MODE_UND,
        .disc_mode=BLE_GAP_DISC_MODE_GEN,.itvl_min=160,.itvl_max=240};
    if(!rc)rc=ble_gap_adv_start(own_type,NULL,BLE_HS_FOREVER,&params,gap,NULL);
    if(rc)ESP_LOGW(TAG,"advertising: %d",rc); // Timer retries without blocking split scanning.
}
static void terminate(void) {
    closing=true;
    atomic_store(&session,0);
    if(conn!=BLE_HS_CONN_HANDLE_NONE)ble_gap_terminate(conn,BLE_ERR_REM_USER_CONN_TERM);
}
static int gap(struct ble_gap_event *event,void *arg) {
    switch(event->type){
    case BLE_GAP_EVENT_CONNECT: {
        if(event->connect.status)return 0; // Timer resumes advertising.
        conn=event->connect.conn_handle;
        reset_link();
        struct ble_gap_conn_desc desc;
        if(ble_gap_conn_find(conn,&desc) ||
           (host_saved?!ble_output_is_host(&desc.peer_id_addr):esp_timer_get_time()>=pairing_until)) {
            terminate();return 0;
        }
        int rc=ble_gap_security_initiate(conn);
        if(rc && rc!=BLE_HS_EALREADY)terminate();
        return 0;
    }
    case BLE_GAP_EVENT_ENC_CHANGE: {
        if(closing)return 0;
        struct ble_gap_conn_desc desc;
        if(event->enc_change.status || ble_gap_conn_find(conn,&desc) || !desc.sec_state.encrypted ||
           !desc.sec_state.bonded || (host_saved && !ble_output_is_host(&desc.peer_id_addr))){terminate();return 0;}
        if(!host_saved){
            esp_err_t rc=nvs_set_blob(storage,"identity",&desc.peer_id_addr,sizeof(desc.peer_id_addr));
            if(rc==ESP_OK)rc=nvs_commit(storage);
            if(rc!=ESP_OK){
                ESP_LOGE(TAG,"Cannot save computer identity: %s",esp_err_to_name(rc));
                ble_store_util_delete_peer(&desc.peer_id_addr);terminate();return 0;
            }
            host_identity=desc.peer_id_addr;host_saved=true;
        }
        encrypted=true;
        readiness();
        struct ble_gap_upd_params params={.itvl_min=6,.itvl_max=12,.latency=0,.supervision_timeout=400};
        ble_gap_update_params(conn,&params); // Host may choose a different interval.
        ESP_LOGI(TAG,"Computer encrypted and bonded");
        return 0;
    }
    case BLE_GAP_EVENT_SUBSCRIBE:
        for(unsigned i=0;i<3;i++)if(event->subscribe.attr_handle==handles[i])
            subscribed[i]=event->subscribe.cur_notify;
        readiness();return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        conn=BLE_HS_CONN_HANDLE_NONE;reset_link();return 0;
    case BLE_GAP_EVENT_REPEAT_PAIRING:
        // Explicit USB reset is required; never silently replace a trusted computer.
        return BLE_GAP_REPEAT_PAIRING_IGNORE;
    default:return 0;
    }
}
static void poll(struct ble_npl_event *event) {
    if(atomic_exchange(&forget_requested,false)) {
        // Remove only the computer bond, never the two split identities.
        if(host_saved){
            terminate();
            int rc=ble_store_util_delete_peer(&host_identity);
            esp_err_t saved=rc?ESP_FAIL:nvs_erase_key(storage,"identity");
            if(saved==ESP_ERR_NVS_NOT_FOUND)saved=ESP_OK;
            if(saved==ESP_OK)saved=nvs_commit(storage);
            if(saved==ESP_OK){host_saved=false;pairing_until=esp_timer_get_time()+120000000;terminate();atomic_store(&reset_result,2);}
            else {ESP_LOGE(TAG,"Computer bond reset failed; retry via USB");atomic_store(&reset_result,3);}
        } else {pairing_until=esp_timer_get_time()+120000000;atomic_store(&reset_result,2);}
    }
    if(atomic_exchange(&queue_failed,false)) {
        ESP_LOGW(TAG,"HID queue full: disconnect computer to release keys");terminate();
    }
    // Preserve press/release order. Never collapse a fast tap into its final release.
    struct queued_report report;
    for(unsigned budget=0;budget<8 && xQueuePeek(reports,&report,0)==pdTRUE;budget++) {
        uint32_t active=atomic_load(&session);
        if(!active || report.session!=active || report.epoch!=atomic_load(&report_epoch)){
            xQueueReceive(reports,&report,0);continue;
        }
        const struct output *o=&report.output;
        unsigned index=0;
        uint8_t bytes[8];
        unsigned length=output_hid_report(o,bytes,&index);
        if(!length){xQueueReceive(reports,&report,0);continue;}
        if(!subscribed[index]){xQueueReceive(reports,&report,0);continue;}
        struct os_mbuf *om=ble_hs_mbuf_from_flat(bytes,length);
        int rc=om?ble_gatts_notify_custom(conn,handles[index],om):BLE_HS_ENOMEM;
        if(rc){
            if(!blocked_since)blocked_since=esp_timer_get_time();
            if(esp_timer_get_time()-blocked_since>250000){
                ESP_LOGW(TAG,"HID notify stalled: %d",rc);terminate();
            }
            break;
        }
        blocked_since=0;
        memcpy(values[index],bytes,lengths[index]);
        if(index==0)display_status_keyboard(o->mods,o->keys);
        xQueueReceive(reports,&report,0);
    }
    if(!host_saved && esp_timer_get_time()>=pairing_until && ble_gap_adv_active())ble_gap_adv_stop();
    advertise();
    atomic_store(&waiting,!atomic_load(&session) && !suspended &&
                 (conn!=BLE_HS_CONN_HANDLE_NONE || host_saved || esp_timer_get_time()<pairing_until));
    ble_npl_callout_reset(&timer,ble_npl_time_ms_to_ticks32(5));
}
void ble_output_send(const struct output *out,void *context) {
    uint32_t active=atomic_load(&session);
    if(!active || active!=(uint32_t)(uintptr_t)context)return;
    if(out->kind==OUT_RESET){atomic_fetch_add(&report_epoch,1);return;}
    if(out->kind==OUT_WHEEL && out->wheel)
        display_status_activity(out->wheel>0?DISPLAY_HINT_SCROLL_UP:DISPLAY_HINT_SCROLL_DOWN);
    if(out->kind==OUT_CONSUMER && (out->consumer==233 || out->consumer==234))
        display_status_activity(out->consumer==233?DISPLAY_HINT_VOL_UP:DISPLAY_HINT_VOL_DOWN);
    struct queued_report report={.output=*out,.session=active,.epoch=atomic_load(&report_epoch)};
    if(xQueueSend(reports,&report,0)!=pdTRUE)atomic_store(&queue_failed,true);
}
void ble_output_init(void) {
    reports=xQueueCreate(64,sizeof(struct queued_report));configASSERT(reports);
    ESP_ERROR_CHECK(nvs_open("sw_ble_host",NVS_READWRITE,&storage));
    size_t size=sizeof(host_identity);
    esp_err_t rc=nvs_get_blob(storage,"identity",&host_identity,&size);
    if(rc!=ESP_ERR_NVS_NOT_FOUND)ESP_ERROR_CHECK(rc);
    host_saved=rc==ESP_OK && size==sizeof(host_identity);
    ble_svc_gap_init();ble_svc_gatt_init();
    ESP_ERROR_CHECK(ble_svc_gap_device_name_set("Cornix StopWatch BLE"));
    ESP_ERROR_CHECK(ble_svc_gap_device_appearance_set(0x03c1));
    ESP_ERROR_CHECK(ble_gatts_count_cfg(services));
    ESP_ERROR_CHECK(ble_gatts_add_svcs(services));
    ble_npl_callout_init(&timer,nimble_port_get_dflt_eventq(),poll,NULL);
}
void ble_output_sync(uint8_t address_type) {
    own_type=address_type;
    conn=BLE_HS_CONN_HANDLE_NONE;reset_link();
    pairing_until=esp_timer_get_time()+120000000;
    ESP_LOGI(TAG,"Computer pairing: %s",host_saved?"saved computer only":"open for 120 seconds");
    advertise();
    ble_npl_callout_reset(&timer,ble_npl_time_ms_to_ticks32(5));
}
