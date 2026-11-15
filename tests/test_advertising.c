/* SPDX-License-Identifier: MIT
 * Execute the actual transport and generated callbacks/work handler against a
 * legacy advertiser model, including Zephyr's name packing and error semantics.
 * This validates software transitions; it does not simulate iOS or BLE radio.
 */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define IS_ENABLED(x) (x)
#define CONFIG_ZMK_SPLIT 1
#define CONFIG_ZMK_SPLIT_ROLE_CENTRAL 1
#define CONFIG_PROSPECTOR_SPLIT_PARTIAL_BURST_MS 1000
#define CONFIG_PROSPECTOR_SPLIT_PARTIAL_SILENT_MS 9000
#define K_NO_WAIT 0
#define K_MSEC(x) (x)
#define LOG_WRN(...) ((void)0)
#define LOG_DBG(...) ((void)0)
#define LOG_INF(...) ((void)0)
#define BT_LE_AD_GENERAL 2
#define BT_LE_AD_NO_BREDR 4
#define BT_DATA_FLAGS 1
#define BT_DATA_UUID16_SOME 2
#define BT_DATA_NAME_SHORTENED 8
#define BT_DATA_NAME_COMPLETE 9
#define BT_DATA_GAP_APPEARANCE 0x19
#define BT_DATA_MANUFACTURER_DATA 0xff
#define BT_APPEARANCE_HID_KEYBOARD 0x03c1
#define BT_UUID_HIDS_VAL 0x1812
#define BT_UUID_BAS_VAL 0x180f
#define BT_BYTES_LIST_LE16(x) ((x)&0xff), (((x)>>8)&0xff)
#define BT_UUID_16_ENCODE(x) BT_BYTES_LIST_LE16(x)
#define BT_GAP_ADV_FAST_INT_MIN_2 160
#define BT_GAP_ADV_FAST_INT_MAX_2 240
#define ZMK_SATELLITE_ADV_MIN 160
#define ZMK_SATELLITE_ADV_MAX 240
#define BURST_INTERVAL_MS 20
#define BURST_COUNT 5
enum {BT_LE_ADV_OPT_CONNECTABLE=1, BT_LE_ADV_OPT_ONE_TIME=2,
      BT_LE_ADV_OPT_USE_NAME=8, BT_LE_ADV_OPT_SCANNABLE=512,
      BT_LE_ADV_OPT_FORCE_NAME_IN_AD=1<<18};
#if defined(BT_LE_ADV_OPT_FORCE_NAME_IN_AD) || defined(BT_LE_ADV_OPT_SCANNABLE)
#error Test options must be enums, like the actual Zephyr API
#endif
struct bt_data {uint8_t type, data_len; const uint8_t *data;};
#define BT_DATA(t,d,n) {.type=t,.data_len=n,.data=(const uint8_t *)(d)}
#define BT_DATA_BYTES(t,...) BT_DATA(t, ((const uint8_t[]){__VA_ARGS__}), sizeof((const uint8_t[]){__VA_ARGS__}))
struct bt_le_adv_param {uint32_t options; uint16_t interval_min, interval_max;};
static bool connected, split_ready=true;
static bool adv_started=true, prospector_adv_active, prospector_adv_connectable, zmk_adv_was_active;
static bool is_active=true;
static uint8_t manufacturer_data[26];
static int burst_remaining;
static int atomic_get(int *p) {return *p;}
static void atomic_dec(int *p) {--*p;}
static bool zmk_ble_active_profile_is_connected(void) {return connected;}
static struct {bool exists, running; uint32_t options; uint8_t ad[31],sd[31]; size_t an,sn;} ctl;
static int update_error, start_error, stop_error, start_calls, stop_calls;
static const char *device_name="iOra";

static bool has_name(const struct bt_data *d,size_t n)
{
    for(size_t i=0;i<n;i++) if(d[i].type==8||d[i].type==9)return true;
    return false;
}
static int pack(uint8_t *out,size_t *len,const struct bt_data *d,size_t n,bool automatic)
{
    *len=0;
    for(size_t i=0;i<n+(automatic?1:0);i++) {
        uint8_t type=i<n?d[i].type:BT_DATA_NAME_COMPLETE;
        size_t size=i<n?d[i].data_len:strlen(device_name);
        const uint8_t *data=i<n?d[i].data:(const uint8_t *)device_name;
        if(*len+size+2>31) {
            if(type!=BT_DATA_NAME_COMPLETE||*len+2>=31)return -EINVAL;
            size=31-*len-2;type=BT_DATA_NAME_SHORTENED;
        }
        out[(*len)++]=(uint8_t)(size+1);out[(*len)++]=type;
        memcpy(out+*len,data,size);*len+=size;
    }
    return 0;
}
static int write_data(const struct bt_data *a,size_t an,const struct bt_data *s,size_t sn)
{
    bool auto_name=ctl.options&BT_LE_ADV_OPT_USE_NAME;
    if(auto_name&&(has_name(a,an)||has_name(s,sn)))return -EINVAL;
    bool force=auto_name&&(ctl.options&BT_LE_ADV_OPT_FORCE_NAME_IN_AD);
    int err=pack(ctl.ad,&ctl.an,a,an,force);
    if(err)return err;
    return pack(ctl.sd,&ctl.sn,s,sn,auto_name&&!force);
}
static int bt_le_adv_update_data(const struct bt_data *a,size_t an,const struct bt_data *s,size_t sn)
{
    if(update_error){int e=update_error;update_error=0;return e;}
    if(!ctl.exists)return -EINVAL;
    if(!ctl.running)return -EAGAIN;
    return write_data(a,an,s,sn);
}
static int bt_le_adv_start(const struct bt_le_adv_param *p,const struct bt_data *a,size_t an,const struct bt_data *s,size_t sn)
{
    start_calls++;
    if(start_error){int e=start_error;start_error=0;return e;}
    if(ctl.running)return -EALREADY;
    ctl.options=p->options;ctl.exists=true;
    int err=write_data(a,an,s,sn);ctl.running=!err;return err;
}
static int bt_le_adv_stop(void)
{
    stop_calls++;
    if(stop_error){int e=stop_error;stop_error=0;return e;}
    ctl.exists=ctl.running=false;return 0;
}
#include "advertising.inc"
static const struct bt_le_adv_param burst_adv_params={.options=0,.interval_min=160,.interval_max=240};

enum zmk_activity_state {ZMK_ACTIVITY_ACTIVE,ZMK_ACTIVITY_IDLE,ZMK_ACTIVITY_SLEEP};
static enum zmk_activity_state last_activity_state;
struct zmk_activity_state_changed {enum zmk_activity_state state;};
typedef struct zmk_activity_state_changed zmk_event_t;
static const struct zmk_activity_state_changed *as_zmk_activity_state_changed(const zmk_event_t *e){return e;}
#define ZMK_EV_EVENT_BUBBLE 0
struct k_work {int unused;};
static struct k_work adv_work;
static int scheduled=-1;
static uint32_t now;
static uint32_t k_uptime_get_32(void){return now;}
static int k_work_schedule(struct k_work *w,int delay){(void)w;scheduled=delay;return 0;}
static int k_work_reschedule(struct k_work *w,int delay){return k_work_schedule(w,delay);}
static int k_work_cancel_delayable(struct k_work *w){(void)w;scheduled=-1;return 0;}
enum {BT_CONN_ROLE_CENTRAL,BT_CONN_ROLE_PERIPHERAL};
struct bt_conn {int role;};
struct bt_conn_info {int role;};
static int bt_conn_get_info(struct bt_conn *c,struct bt_conn_info *i){i->role=c->role;return 0;}
static bool prospector_split_fully_connected(void){return split_ready;}
static void build_manufacturer_payload(void){}
static uint32_t get_current_update_interval(void){return connected&&is_active?200:30000;}
#include "generated_callbacks.inc"

static const uint8_t *field(uint8_t type,const uint8_t *d,size_t n,size_t *size)
{
    for(size_t i=0;i<n;i+=d[i]+1)if(d[i+1]==type){*size=d[i]-1;return d+i+2;}
    return NULL;
}
static bool present(uint8_t type,const uint8_t *d,size_t n){size_t s;return field(type,d,n,&s)!=NULL;}
static void assert_status(void)
{
    size_t n=0;const uint8_t *p=field(0xff,ctl.ad,ctl.an,&n);
    if(!p)p=field(0xff,ctl.sd,ctl.sn,&n);
    assert(p&&n==26&&!memcmp(p,manufacturer_data,26));
}
static void assert_pairing(void)
{
    assert(ctl.running&&(ctl.options&BT_LE_ADV_OPT_CONNECTABLE));
    size_t n=0;const uint8_t *uuid=field(2,ctl.ad,ctl.an,&n);
    if(!uuid)uuid=field(2,ctl.sd,ctl.sn,&n);
    assert(uuid&&n==4&&uuid[0]==0x12&&uuid[1]==0x18);
    const uint8_t *appearance=field(0x19,ctl.ad,ctl.an,&n);
    if(!appearance)appearance=field(0x19,ctl.sd,ctl.sn,&n);
    assert(appearance&&n==2&&appearance[0]==0xc1&&appearance[1]==3);
    assert(present(8,ctl.ad,ctl.an)||present(9,ctl.ad,ctl.an)||present(8,ctl.sd,ctl.sn)||present(9,ctl.sd,ctl.sn));
    assert(ctl.an<=31&&ctl.sn<=31);assert_status();
}
static void reset(void)
{
    memset(&ctl,0,sizeof(ctl));prospector_adv_active=prospector_adv_connectable=zmk_adv_was_active=false;
    update_error=start_error=stop_error=start_calls=stop_calls=0;
    connected=false;split_ready=true;burst_remaining=0;adv_cycle_counter=0;
    scheduled=-1;now=0;is_active=true;last_activity_state=ZMK_ACTIVITY_ACTIVE;
    device_name="iOra";strcpy(name_adv_buffer,device_name);name_ad[1].data_len=4;
    for(unsigned i=0;i<26;i++)manufacturer_data[i]=(uint8_t)i;
}
static void tick(void){adv_work_handler(&adv_work);}
static void connect_host(void)
{
    struct bt_conn c={BT_CONN_ROLE_PERIPHERAL};
    connected=true;ctl.running=ctl.exists=false;
    prospector_ble_connected(&c,0);
}

/* ZMK v0.3 update_advertising's NONE/CONN transitions. On -EALREADY it
 * deliberately keeps NONE; selecting an already tracked CONN does no HCI call.
 * Keep this model explicit: the actual ZMK application is checked by builds. */
static bool zmk_tracks_connection_ad;
static int zmk_update(void)
{
    if(connected&&zmk_tracks_connection_ad) {
        int err=bt_le_adv_stop();zmk_tracks_connection_ad=false;return err;
    }
    if(!connected&&!zmk_tracks_connection_ad) {
        const struct bt_le_adv_param params={
            .options=BT_LE_ADV_OPT_CONNECTABLE|BT_LE_ADV_OPT_ONE_TIME|BT_LE_ADV_OPT_USE_NAME
#if SATELLITE_HAS_FORCE_NAME_IN_AD
            |BT_LE_ADV_OPT_FORCE_NAME_IN_AD
#endif
        };
        int err=bt_le_adv_start(&params,zmk_ad_restore,ARRAY_SIZE(zmk_ad_restore),NULL,0);
        if(!err)zmk_tracks_connection_ad=true;
        return err;
    }
    return 0;
}

int main(void)
{
    /* Startup: piggyback on the actual ZMK-style auto-name options. */
    reset();ctl.exists=ctl.running=true;
    ctl.options=BT_LE_ADV_OPT_CONNECTABLE|BT_LE_ADV_OPT_ONE_TIME|BT_LE_ADV_OPT_USE_NAME;
#if SATELLITE_HAS_FORCE_NAME_IN_AD
    ctl.options|=BT_LE_ADV_OPT_FORCE_NAME_IN_AD;
    /* Reproduce the old 31-byte status AD + forced auto-name overflow. */
    assert(write_data(prospector_ad,ARRAY_SIZE(prospector_ad),NULL,0)==-EINVAL);
#endif
    tick();assert_pairing();assert(!prospector_adv_active&&start_calls==0);

    /* No legacy set after a stop: -EINVAL must also reach proxy start. */
    reset();tick();assert_pairing();assert(prospector_adv_connectable);
    assert(ctl.options&BT_LE_ADV_OPT_ONE_TIME);
    is_active=false;for(int i=0;i<6;i++){tick();assert_pairing();}

    /* Connected A -> empty B (ZMK start raced with own NC and got EALREADY). */
    reset();connected=true;tick();assert(prospector_adv_active&&!prospector_adv_connectable);
    assert_status();connected=false;tick();assert_pairing();
    /* Pair on B, automatically end proxy, then use NC status. */
    connect_host();assert(!prospector_adv_active&&scheduled==0);tick();assert_status();
    assert(!(ctl.options&BT_LE_ADV_OPT_CONNECTABLE));

    /* Bonded offline profile and successful reconnection use the same proxy. */
    connected=false;tick();assert_pairing();connect_host();tick();assert_status();
    /* Successful disconnect handoff followed by ZMK start: never stop ZMK's set. */
    struct bt_conn host_conn={BT_CONN_ROLE_PERIPHERAL};
    connected=false;prospector_ble_disconnected(&host_conn,0);
    int stopped=stop_calls;ctl.exists=ctl.running=true;
    ctl.options=BT_LE_ADV_OPT_CONNECTABLE|BT_LE_ADV_OPT_ONE_TIME|BT_LE_ADV_OPT_USE_NAME;
#if SATELLITE_HAS_FORCE_NAME_IN_AD
    ctl.options|=BT_LE_ADV_OPT_FORCE_NAME_IN_AD;
#endif
    tick();assert_pairing();assert(stop_calls==stopped);

    /* Start/stop/update failures recover; failed stop retains ownership. */
    reset();start_error=-ENOMEM;tick();assert(!prospector_adv_active&&scheduled<=1000);
    tick();assert_pairing();connected=true;stop_error=-EIO;tick();
    assert(prospector_adv_active&&prospector_adv_connectable&&scheduled<=1000);
    tick();assert_status();assert(!prospector_adv_connectable);
    update_error=-EINVAL;tick();assert_status();assert(prospector_adv_active);
    update_error=-EIO;tick();assert(prospector_adv_active&&scheduled<=1000);tick();assert_status();
    reset();start_error=-EALREADY;tick();assert(!prospector_adv_active&&scheduled<=1000);tick();assert_pairing();
    reset();ctl.exists=true;ctl.running=false;tick();assert_pairing(); /* -EAGAIN */

    /* Long names stay within both 31-byte budgets, including name-only NC. */
    reset();device_name="ABCDEFGHIJKLMNOPQRSTUVWXYZ12345";
    memcpy(name_adv_buffer,device_name,26);name_adv_buffer[26]=0;name_ad[1].data_len=26;
    tick();assert_pairing();assert(ctl.an==31&&ctl.sn==28);
    connect_host();is_active=false;tick();tick();tick();assert(ctl.an<=31&&ctl.sn<=31);
    assert(present(9,ctl.ad,ctl.an)); /* Periodic direct name broadcast */
    tick();assert_status();

    /* Sleep stays silent even with a queued callback; wake recovers. */
    zmk_event_t e={ZMK_ACTIVITY_SLEEP};activity_state_listener(&e);assert(!ctl.running);
    scheduled=-1;tick();assert(!ctl.running&&scheduled==-1);
    e.state=ZMK_ACTIVITY_ACTIVE;activity_state_listener(&e);assert(scheduled==500);tick();assert_status();

    /* Partial split radio-silent windows survive; split callback does not
     * clear ownership of the still-running advertiser. */
    reset();split_ready=false;tick();assert_status();
    struct bt_conn peer={BT_CONN_ROLE_CENTRAL};prospector_ble_connected(&peer,0);assert(prospector_adv_active);
    now=1001;tick();assert(!ctl.running&&scheduled==8999);
    now=10000;tick();assert_status();split_ready=true;tick();assert_pairing();

    /* USB-ready flags remain in the exact same status payload; USB alone
     * does not suppress the host's connectable advertising. */
    reset();manufacturer_data[9]=0x0c;tick();assert_pairing();assert_status();

    /* End-to-end modeled ZMK handoff, including its no-retry EALREADY path. */
    reset();zmk_tracks_connection_ad=false;assert(zmk_update()==0);tick();assert_pairing();
    connected=true;ctl.running=ctl.exists=false;assert(zmk_update()==0);
    struct bt_conn c={BT_CONN_ROLE_PERIPHERAL};prospector_ble_connected(&c,0);tick();assert_status();
    connected=false;assert(zmk_update()==-EALREADY);assert(!zmk_tracks_connection_ad);
    tick();assert_pairing();connect_host();assert(zmk_update()==0);tick();assert_status();
    connected=false;assert(zmk_update()==-EALREADY);tick();assert_pairing();
    /* Simulate another component replacing the advertiser before our next
     * callback: host completion must not stop its new connection advertiser. */
    ctl.running=ctl.exists=false;prospector_ble_connected(&c,0);
    assert(zmk_update()==0);int before_stop=stop_calls;tick();assert_pairing();assert(stop_calls==before_stop);
    puts("advertising: startup, profile switches, reconnect, errors, names, sleep and split assertions passed");
    return 0;
}
