// SPDX-License-Identifier: MIT
// ZMK split service UUID / packet ABI: see README.md Protocol section.
#include "dongle.h"
#include "ble_output.h"
#include "display_status.h"
#include "zmk_compat.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "host/ble_store.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG="split";
#define SPLIT_UUID(n) BLE_UUID128_INIT(0x2a,0x48,0xc2,0xb1,0xcf,0xc5,0x67,0xc9,0x07,0x71,0x96,0x00,n,0,0,0)
static const ble_uuid128_t service_uuid=SPLIT_UUID(0), position_uuid=SPLIT_UUID(1),
    sensor_uuid=SPLIT_UUID(3), layout_uuid=SPLIT_UUID(5);
struct peer {
    bool used;
    uint16_t conn, start, end, pos, sensor, layout;
    uint16_t pos_end, sensor_end, pos_ccc, sensor_ccc;
    bool discovering_sensor;
    uint16_t battery_start, battery_end, battery_value;
    bool battery_busy;
};
static struct peer peers[2];
static uint8_t own_addr_type;
static struct ble_npl_callout scan_timer;
static struct ble_npl_callout battery_timer;
static int64_t pairing_deadline;
void ble_store_config_init(void);
static int gap_event(struct ble_gap_event *event, void *arg);
static void battery_discover(struct peer *p);
static const struct ble_gap_conn_params split_params={
    .scan_itvl=0x10,.scan_window=0x10,
    .itvl_min=SPLIT_INTERVAL,.itvl_max=SPLIT_INTERVAL,
    .latency=SPLIT_LATENCY,.supervision_timeout=SPLIT_TIMEOUT,
};
static void log_connection(uint16_t conn) {
    struct ble_gap_conn_desc desc;
    if(ble_gap_conn_find(conn,&desc))return;
    ESP_LOGI(TAG,"Split %u: interval=%u (1.25ms), latency=%u, timeout=%u (10ms)",
             conn,desc.conn_itvl,desc.conn_latency,desc.supervision_timeout);
    if(desc.conn_itvl!=SPLIT_INTERVAL || desc.conn_latency!=SPLIT_LATENCY ||
       desc.supervision_timeout!=SPLIT_TIMEOUT)
        ESP_LOGW(TAG,"Split %u differs from ZMK preferred parameters",conn);
}
static void scan_later(void) { ble_npl_callout_reset(&scan_timer,ble_npl_time_ms_to_ticks32(500)); }

static struct peer *find_peer(uint16_t conn) {
    for (unsigned i=0; i<2; i++) if (peers[i].used && peers[i].conn==conn) return &peers[i];
    return NULL;
}
static void fail(struct peer *p, const char *stage, int rc) {
    ESP_LOGW(TAG,"%s failed: %d",stage,rc);
    if (p) ble_gap_terminate(p->conn,BLE_ERR_REM_USER_CONN_TERM);
}
static void input(struct peer *p, enum input_kind kind, struct os_mbuf *om) {
    struct input_event e={.kind=kind,.peer=p-peers,.at=esp_timer_get_time()/1000};
    if (om) {
        unsigned len=OS_MBUF_PKTLEN(om);
        if (len>16) { fail(p,"packet length",len); return; }
        e.len=len;
        if (os_mbuf_copydata(om,0,len,e.bytes)) return;
    }
    if (kind==INPUT_STATE && e.len==16) display_status_positions(e.peer,e.bytes);
    if (kind==INPUT_SENSOR && e.len==14 && e.bytes[0]<2 && e.bytes[1]==1)
        display_status_side(e.peer,e.bytes[0]);
    if (kind==INPUT_DISCONNECT) display_status_disconnect(e.peer);
    dongle_input(&e);
}
static int read_state(uint16_t conn,const struct ble_gatt_error *error,struct ble_gatt_attr *attr,void *arg) {
    struct peer *p=arg;
    if (error->status) fail(p,"read positions",error->status);
    else if (OS_MBUF_PKTLEN(attr->om)!=16) fail(p,"position ABI",-1);
    else {
        input(p,INPUT_STATE,attr->om);
        display_status_ready(p-peers);
        ESP_LOGI(TAG,"Peer %d ready",(int)(p-peers));
        battery_discover(p); // Optional; failures never tear down keyboard input.
    }
    return 0;
}
static int subscribed_sensor(uint16_t conn,const struct ble_gatt_error *error,struct ble_gatt_attr *attr,void *arg) {
    struct peer *p=arg;
    if (error->status) { fail(p,"sensor CCC",error->status); return 0; }
    int rc=ble_gattc_read(conn,p->pos,read_state,p);
    if (rc) fail(p,"read start",rc);
    return 0;
}
static int subscribed_position(uint16_t conn,const struct ble_gatt_error *error,struct ble_gatt_attr *attr,void *arg) {
    struct peer *p=arg;
    if (error->status) { fail(p,"position CCC",error->status); return 0; }
    if (!p->sensor) return subscribed_sensor(conn,error,attr,arg);
    if (!p->sensor_ccc) { fail(p,"missing sensor CCC",-1); return 0; }
    uint8_t on[]={1,0};
    int rc=ble_gattc_write_flat(conn,p->sensor_ccc,on,2,subscribed_sensor,p);
    if (rc) fail(p,"subscribe sensor",rc);
    return 0;
}
static void subscribe(struct peer *p) {
    uint8_t on[]={1,0};
    int rc=ble_gattc_write_flat(p->conn,p->pos_ccc,on,2,subscribed_position,p);
    if (rc) fail(p,"subscribe position",rc);
}
static int layout_selected(uint16_t conn,const struct ble_gatt_error *error,struct ble_gatt_attr *attr,void *arg) {
    struct peer *p=arg;
    if (error->status) fail(p,"select layout 50",error->status);
    else subscribe(p);
    return 0;
}
static int descriptors(uint16_t conn,const struct ble_gatt_error *error,uint16_t chr,const struct ble_gatt_dsc *dsc,void *arg) {
    struct peer *p=arg;
    if (!error->status) {
        if (ble_uuid_u16(&dsc->uuid.u)==BLE_GATT_DSC_CLT_CFG_UUID16) {
            if (chr==p->pos) p->pos_ccc=dsc->handle;
            if (chr==p->sensor) p->sensor_ccc=dsc->handle;
        }
        return 0;
    }
    if (error->status!=BLE_HS_EDONE) { fail(p,"descriptors",error->status); return 0; }
    if (!p->pos_ccc) { fail(p,"missing position CCC",-1); return 0; }
    if (p->sensor && !p->discovering_sensor) {
        p->discovering_sensor=true;
        int rc=ble_gattc_disc_all_dscs(conn,p->sensor,p->sensor_end,descriptors,p);
        if (rc) fail(p,"sensor descriptors",rc);
        return 0;
    }
    if (p->layout) {
        uint8_t layout=0;
        int rc=ble_gattc_write_flat(conn,p->layout,&layout,1,layout_selected,p);
        if (rc) fail(p,"select layout",rc);
    } else subscribe(p);
    return 0;
}
static int characteristics(uint16_t conn,const struct ble_gatt_error *error,const struct ble_gatt_chr *chr,void *arg) {
    struct peer *p=arg;
    if (!error->status) {
        if (p->pos && !p->pos_end) p->pos_end=chr->def_handle-1;
        if (p->sensor && !p->sensor_end) p->sensor_end=chr->def_handle-1;
        if (!ble_uuid_cmp(&chr->uuid.u,&position_uuid.u)) p->pos=chr->val_handle;
        if (!ble_uuid_cmp(&chr->uuid.u,&sensor_uuid.u)) p->sensor=chr->val_handle;
        if (!ble_uuid_cmp(&chr->uuid.u,&layout_uuid.u)) p->layout=chr->val_handle;
        return 0;
    }
    if (error->status!=BLE_HS_EDONE || !p->pos) { fail(p,"characteristics",error->status); return 0; }
    if (!p->pos_end) p->pos_end=p->end;
    if (!p->sensor_end) p->sensor_end=p->end;
    int rc=ble_gattc_disc_all_dscs(conn,p->pos,p->pos_end,descriptors,p);
    if (rc) fail(p,"discover descriptors",rc);
    return 0;
}
static int service(uint16_t conn,const struct ble_gatt_error *error,const struct ble_gatt_svc *svc,void *arg) {
    struct peer *p=arg;
    if (!error->status) { p->start=svc->start_handle; p->end=svc->end_handle; return 0; }
    if (error->status!=BLE_HS_EDONE || !p->start) { fail(p,"service",error->status); return 0; }
    int rc=ble_gattc_disc_all_chrs(conn,p->start,p->end,characteristics,p);
    if (rc) fail(p,"discover characteristics",rc);
    return 0;
}
static void scan(struct ble_npl_event *event) {
    if (peers[0].used && peers[1].used) return;
    if (ble_gap_disc_active() || ble_gap_conn_active()) return;
    struct ble_gap_disc_params params={.passive=1,.filter_duplicates=1,.itvl=0x60,.window=0x30};
    int rc=ble_gap_disc(own_addr_type,5000,&params,gap_event,NULL);
    if (rc) { ESP_LOGW(TAG,"scan: %d",rc); scan_later(); }
}
static bool bonded(const ble_addr_t *address) {
    ble_addr_t bonds[3]; int count=0;
    if (ble_store_util_bonded_peers(bonds,&count,3)) return false;
    for (int i=0;i<count;i++) if (!ble_output_is_host(&bonds[i]) && !ble_addr_cmp(address,&bonds[i])) return true;
    return false;
}

static int battery_read(uint16_t conn,const struct ble_gatt_error *error,struct ble_gatt_attr *attr,void *arg) {
    struct peer *p=arg;
    if (!p->used || p->conn!=conn) return 0;
    p->battery_busy=false;
    uint8_t level=255;
    if (!error->status && attr && OS_MBUF_PKTLEN(attr->om)==1)
        os_mbuf_copydata(attr->om,0,1,&level);
    display_status_battery(p-peers,level<=100?level:-1);
    return 0;
}
static void battery_request(struct peer *p) {
    if (!p->used || !p->battery_value || p->battery_busy) return;
    p->battery_busy=true;
    int rc=ble_gattc_read(p->conn,p->battery_value,battery_read,p);
    if (rc) {p->battery_busy=false;display_status_battery(p-peers,-1);}
}
static int battery_characteristic(uint16_t conn,const struct ble_gatt_error *error,const struct ble_gatt_chr *chr,void *arg) {
    struct peer *p=arg;
    if (!p->used || p->conn!=conn) return 0;
    if (!error->status) {p->battery_value=chr->val_handle;return 0;}
    p->battery_busy=false;
    if (error->status==BLE_HS_EDONE) battery_request(p);
    return 0;
}
static int battery_service(uint16_t conn,const struct ble_gatt_error *error,const struct ble_gatt_svc *svc,void *arg) {
    struct peer *p=arg;
    if (!p->used || p->conn!=conn) return 0;
    if (!error->status) {p->battery_start=svc->start_handle;p->battery_end=svc->end_handle;return 0;}
    if (error->status!=BLE_HS_EDONE || !p->battery_start) {p->battery_busy=false;return 0;}
    int rc=ble_gattc_disc_chrs_by_uuid(conn,p->battery_start,p->battery_end,
                                      BLE_UUID16_DECLARE(0x2a19),battery_characteristic,p);
    if (rc) p->battery_busy=false;
    return 0;
}
static void battery_discover(struct peer *p) {
    p->battery_busy=true;
    int rc=ble_gattc_disc_svc_by_uuid(p->conn,BLE_UUID16_DECLARE(0x180f),battery_service,p);
    if (rc) p->battery_busy=false;
}
static void battery_poll(struct ble_npl_event *event) {
    for (unsigned i=0;i<2;i++) battery_request(&peers[i]);
    ble_npl_callout_reset(&battery_timer,ble_npl_time_ms_to_ticks32(30000));
}
static int gap_event(struct ble_gap_event *event,void *arg) {
    struct peer *p;
    int rc;
    switch (event->type) {
    case BLE_GAP_EVENT_DISC: {
        struct ble_hs_adv_fields fields;
        if (ble_hs_adv_parse_fields(&fields,event->disc.data,event->disc.length_data)) return 0;
        bool match=false;
        // Bonded ZMK halves advertise directly, without service UUID data.
        if (event->disc.event_type==BLE_HCI_ADV_RPT_EVTYPE_DIR_IND && bonded(&event->disc.addr)) match=true;
        for (int i=0;i<fields.num_uuids128;i++)
            if (!ble_uuid_cmp(&fields.uuids128[i].u,&service_uuid.u)) match=true;
        if (!match || (peers[0].used && peers[1].used)) return 0;
        if (esp_timer_get_time()>pairing_deadline && !bonded(&event->disc.addr)) return 0;
        struct ble_gap_conn_desc existing;
        if (!ble_gap_conn_find_by_addr(&event->disc.addr,&existing)) return 0;
        if (event->disc.event_type!=BLE_HCI_ADV_RPT_EVTYPE_ADV_IND &&
            event->disc.event_type!=BLE_HCI_ADV_RPT_EVTYPE_DIR_IND) return 0;
        ble_gap_disc_cancel();
        rc=ble_gap_connect(own_addr_type,&event->disc.addr,10000,&split_params,gap_event,NULL);
        if (rc) scan_later();
        return 0;
    }
    case BLE_GAP_EVENT_DISC_COMPLETE: scan_later(); return 0;
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status) { scan_later(); return 0; }
        p=!peers[0].used?&peers[0]:!peers[1].used?&peers[1]:NULL;
        if (!p) { ble_gap_terminate(event->connect.conn_handle,BLE_ERR_REM_USER_CONN_TERM); return 0; }
        memset(p,0,sizeof(*p)); p->used=true; p->conn=event->connect.conn_handle;
        struct ble_gap_conn_desc desc;
        rc=ble_gap_conn_find(p->conn,&desc);
        if (rc || (!bonded(&desc.peer_id_addr) && esp_timer_get_time()>pairing_deadline)) {
            fail(p,"pairing window closed",rc); return 0;
        }
        uint8_t identity[7]={desc.peer_id_addr.type};
        memcpy(identity+1,desc.peer_id_addr.val,6);
        display_status_connect(p-peers,identity);
        log_connection(p->conn);
        rc=ble_gap_security_initiate(p->conn);
        if (rc) fail(p,"start encryption",rc);
        scan_later();
        return 0;
    case BLE_GAP_EVENT_CONN_UPDATE:
        if(find_peer(event->conn_update.conn_handle)) {
            if(event->conn_update.status)ESP_LOGW(TAG,"Split parameter update: %d",event->conn_update.status);
            log_connection(event->conn_update.conn_handle);
        }
        return 0;
    case BLE_GAP_EVENT_ENC_CHANGE:
        p=find_peer(event->enc_change.conn_handle);
        if (!p) return 0;
        if (event->enc_change.status) { fail(p,"encryption",event->enc_change.status); return 0; }
        rc=ble_gattc_disc_svc_by_uuid(p->conn,&service_uuid.u,service,p);
        if (rc) fail(p,"discover split service",rc);
        return 0;
    case BLE_GAP_EVENT_NOTIFY_RX:
        p=find_peer(event->notify_rx.conn_handle);
        if (!p) return 0;
        if (event->notify_rx.attr_handle==p->pos) input(p,INPUT_STATE,event->notify_rx.om);
        else if (event->notify_rx.attr_handle==p->sensor) input(p,INPUT_SENSOR,event->notify_rx.om);
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        p=find_peer(event->disconnect.conn.conn_handle);
        if (p) { input(p,INPUT_DISCONNECT,NULL); p->used=false; }
        scan_later();
        return 0;
    case BLE_GAP_EVENT_REPEAT_PAIRING:
        // Do not silently replace an existing trusted identity. Clear bonds explicitly.
        return BLE_GAP_REPEAT_PAIRING_IGNORE;
    default: return 0;
    }
}
static void synced(void) {
    ESP_ERROR_CHECK(ble_hs_util_ensure_addr(0));
    ESP_ERROR_CHECK(ble_hs_id_infer_auto(0,&own_addr_type));
    ble_addr_t bonds[3]; int count=0;
    ESP_ERROR_CHECK(ble_store_util_bonded_peers(bonds,&count,3));
    for(int i=0;i<count;i++)if(ble_output_is_host(&bonds[i])){count--;break;}
    pairing_deadline=count<2 ? esp_timer_get_time()+60000000 : 0;
    display_status_pairing(pairing_deadline?(uint32_t)(pairing_deadline/1000):0);
    ESP_LOGI(TAG,"%d saved peers; new pairing %s",count,count<2?"open for 60s":"disabled");
    ble_output_sync(own_addr_type);
    scan_later();
    ble_npl_callout_reset(&battery_timer,ble_npl_time_ms_to_ticks32(30000));
}
static void host(void *arg) { nimble_port_run(); nimble_port_freertos_deinit(); }
void split_start(void) {
    ESP_ERROR_CHECK(nimble_port_init());
    ble_hs_cfg.sync_cb=synced;
    ble_hs_cfg.sm_io_cap=BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_bonding=1;
    ble_hs_cfg.sm_sc=1;
    ble_hs_cfg.sm_mitm=0;
    ble_hs_cfg.sm_our_key_dist=BLE_SM_PAIR_KEY_DIST_ENC|BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist=BLE_SM_PAIR_KEY_DIST_ENC|BLE_SM_PAIR_KEY_DIST_ID;
    ble_store_config_init();
    ble_output_init();
    ble_npl_callout_init(&scan_timer,nimble_port_get_dflt_eventq(),scan,NULL);
    ble_npl_callout_init(&battery_timer,nimble_port_get_dflt_eventq(),battery_poll,NULL);
    nimble_port_freertos_init(host);
}
