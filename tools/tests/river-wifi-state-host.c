#include <assert.h>
#include <sys/types.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#define LCK_GRP_DECLARE(a,b) int a __attribute__((unused))
#define LCK_MTX_DECLARE(a,b) int a=1
#define Z_WAITOK 0
#define Z_ZERO 0
#define KEV_VENDOR_APPLE 1
#define KEV_IEEE80211_CLASS 1
#ifndef __DECONST
#define __DECONST(type,p) ((type)(uintptr_t)(const void *)(p))
#endif
static int engine_lock_depth,net_lock_depth;
static void lck_mtx_lock(int *lock) {int *depth=*lock==1?&engine_lock_depth:&net_lock_depth;assert(!*depth);(*depth)++;}
static void lck_mtx_unlock(int *lock) {int *depth=*lock==1?&engine_lock_depth:&net_lock_depth;assert(*depth==1);(*depth)--;}
static void *kalloc_data(size_t n,int flags) {(void)flags;return calloc(1,n);}
static void kfree_data(void *p,size_t n) {(void)n;free(p);}
static int copyin(user_addr_t src,void *dst,size_t n) {if(!src)return EFAULT;memcpy(dst,(void*)src,n);return 0;}
static int copyout(const void *src,user_addr_t dst,size_t n) {if(!dst)return EFAULT;memcpy((void*)dst,src,n);return 0;}
static int host_memset_s(void *p,size_t n,int value,size_t count) {assert(count<=n);memset(p,value,count);return 0;}
#define memset_s host_memset_s
struct kev_msg {uint32_t vendor_code,kev_class,kev_subclass,event_code;struct {size_t data_length;void *data_ptr;}dv[3];};
static unsigned events;
static void kev_post_msg(const struct kev_msg *msg) {assert(msg->event_code);events++;}
#define KERNEL 1
#include "river-wifi-abi.h"
void river_pbkdf2_pmk(const char *pass,const char *ssid,size_t len,uint8_t pmk[32]) {(void)pass;(void)ssid;(void)len;memset(pmk,7,32);}
#include "river-wifi-engine.h"
typedef uintptr_t vm_offset_t;
static struct {uint8_t pmk[32],pending[512];unsigned stage,pending_len;bool associated,keys_installed,data_tx_stalled,pmk_ready;} river_wpa;
static struct {uint8_t ssid[32],bssid[6],rates[12],rsn[22],ap_rsn[257];unsigned channel,beacon,caps,dtim,nrates,rsn_len,ap_rsn_len,ssid_len;bool found,enabled,rates_ok,security_ok;} river_target;
static uint8_t river_target_bss;
static vm_offset_t river_smd_smem=1,river_smd_ipc=1;
static uint32_t river_smd_wlan_info=1,river_smd_wlan_fifo=1;
static uint64_t river_connection_generation,river_cleanup_generation;
static struct river_apple80211_join_req river_connection_req;
static struct river_apple80211_target_info river_connection_info;
static bool river_connection_saved,river_cleanup_pending;
static unsigned river_reconnect_wait,river_reconnect_attempts;
static bool river_net_live,river_wifi_ifp=true;
static int net_mutex=2,*river_net_lock=&net_mutex;
#define IFF_UP 1
#define IFF_RUNNING 2
static unsigned cleanup_calls,link_calls,assoc_calls,network_drops,flags_set;
static bool in_rx,assoc_success=true,wpa_success=true,inject_new_join,inject_stop;
static unsigned queued_packets;
static void river_network_create(void) {river_wifi_ifp=true;}
static void river_network_drop(void) {network_drops++;river_net_live=false;queued_packets=0;}
static void river_secret_clear(void *p,unsigned n) {memset(p,0,n);}
static void ifnet_set_flags(bool ifp,unsigned flags,unsigned mask) {
 assert(ifp&&flags==3&&mask==3);flags_set++;
 if(inject_stop){inject_stop=false;struct apple80211req stop={.req_type=22};assert(!set(&stop));}
}
static unsigned river_auth_status,river_assoc_status,river_assoc_aid;
static bool river_auth_seen,river_assoc_seen;
static unsigned river_eapol_seen;
static bool river_mac_equal(const volatile uint8_t *a,const uint8_t *b) {for(unsigned i=0;i<6;i++)if(a[i]!=b[i])return false;return true;}
static unsigned river_le16(const volatile uint8_t *p) {return p[0]|((unsigned)p[1]<<8);}
static void river_wpa_queue(const volatile uint8_t *p,unsigned n) {(void)p;(void)n;}
static bool river_scan_xact(vm_offset_t smem,uint32_t info,uint32_t fifo,vm_offset_t ipc,unsigned type,uint8_t *msg,unsigned len) {
 assert(!in_rx&&smem&&info&&fifo&&ipc&&type==18&&len==9&&msg[8]!=255);cleanup_calls++;return true;
}
static bool river_target_link(vm_offset_t smem,uint32_t info,uint32_t fifo,vm_offset_t ipc,unsigned state) {assert(!in_rx&&smem&&info&&fifo&&ipc&&!state);link_calls++;return true;}
static void queue_join(const char *ssid,const uint8_t *bssid);
static void river_target_associate(vm_offset_t smem,uint32_t info,uint32_t fifo,vm_offset_t ipc) {
 assert(!in_rx&&smem&&info&&fifo&&ipc);assoc_calls++;river_target_bss=9;river_wpa.associated=assoc_success;
}
static void river_wpa_run(vm_offset_t smem,uint32_t info,uint32_t fifo,vm_offset_t ipc) {
 assert(!in_rx&&smem&&info&&fifo&&ipc);
 if(inject_new_join){inject_new_join=false;queue_join("two",NULL);}
 if(river_wpa.associated&&wpa_success){river_wpa.stage=2;river_wpa.keys_installed=true;}
}
#include "river-wifi-rx.h"
#include "river-wifi-worker.h"
static const uint8_t ap_a[6]={2,1,1,1,1,1},ap_b[6]={2,2,2,2,2,2};
static void queue_join(const char *ssid,const uint8_t *bssid) {
 struct apple80211_assoc asked={0};asked.ssid_length=(uint32_t)strlen(ssid);memcpy(asked.ssid,ssid,asked.ssid_length);
 if(bssid)memcpy(asked.bssid,bssid,6);asked.key_length=32;memset(asked.key,3,32);
 struct apple80211req request={.req_type=20,.req_len=sizeof(asked),.req_data=(uintptr_t)&asked};assert(!set(&request));
}
static void setup(void) {
 river_bss_count=scan_result_idx=0;scan_pending_flag=join_pending_flag=stop_pending_flag=false;
 join_result=0;join_generation=0;river_power_state=1;active_phase=PHASE_IDLE;events=0;
 memset(&pending_join,0,sizeof(pending_join));memset(&active_bss,0,sizeof(active_bss));
 memset(&river_target,0,sizeof(river_target));memset(&river_wpa,0,sizeof(river_wpa));
 river_target_bss=255;river_connection_generation=river_cleanup_generation=0;
 memset(&river_connection_req,0,sizeof(river_connection_req));memset(&river_connection_info,0,sizeof(river_connection_info));
 river_connection_saved=river_cleanup_pending=river_net_live=false;river_reconnect_wait=river_reconnect_attempts=0;
 cleanup_calls=link_calls=assoc_calls=network_drops=flags_set=queued_packets=0;in_rx=false;assoc_success=wpa_success=true;inject_new_join=inject_stop=false;
 river_apple80211_add_bss(ap_a,(const uint8_t*)"one",3,2412,-40,100,0,NULL,0);
 river_apple80211_add_bss(ap_b,(const uint8_t*)"two",3,2437,-45,100,0,NULL,0);
}
static void deauth(void) {
 uint8_t frame[26]={0xc0};memcpy(frame+4,river_test_mac,6);memcpy(frame+10,river_target.bssid,6);frame[24]=4;
 in_rx=true;river_target_rx(frame,sizeof(frame),24,0xc0);in_rx=false;
}
int main(void) {
 setup();memcpy(river_target.ssid,"one",3);river_target.ssid_len=3;memcpy(river_target.bssid,ap_a,6);
 river_target.enabled=river_target.found=true;river_wpa.pmk_ready=true;memset(river_wpa.pmk,5,32);
 river_connection_boot();assert(river_net_live&&river_connection_saved&&active_phase==PHASE_JOINED&&assoc_calls==1);
 deauth();river_connection_service();river_reconnect_wait=0;river_connection_service();assert(river_net_live&&assoc_calls==2);
 setup();memcpy(river_target.ssid,"one",3);river_target.ssid_len=3;memcpy(river_target.bssid,ap_a,6);
 river_target.enabled=river_target.found=true;river_wpa.pmk_ready=true;
 struct apple80211req boot_off={.req_type=19,.req_val=0};assert(!set(&boot_off));river_connection_boot();
 assert(!assoc_calls&&!river_net_live&&active_phase==PHASE_IDLE);river_connection_service();assert(!river_target.enabled);
 setup();memcpy(river_target.ssid,"one",3);river_target.ssid_len=3;memcpy(river_target.bssid,ap_a,6);
 river_target.enabled=river_target.found=true;river_wpa.pmk_ready=true;queue_join("two",ap_b);river_connection_boot();
 assert(!assoc_calls&&join_pending_flag&&active_phase==PHASE_JOINING);
 setup();queue_join("one",ap_a);struct river_apple80211_join_req old,current;
 assert(river_apple80211_join_pending(&old));queue_join("two",ap_b);unsigned before=events;
 assert(!river_apple80211_join_complete(&old,NULL,16)&&events==before&&join_pending_flag&&active_phase==PHASE_JOINING);
 assert(river_apple80211_join_pending(&current));struct river_apple80211_target_info info;
 assert(river_apple80211_find_target(&current,&info));assert(river_apple80211_join_complete(&current,&info,0));
 assert(active_phase==PHASE_JOINED&&join_result==0&&!memcmp(active_bss.bssid,ap_b,6));
 assert(!river_apple80211_join_complete(&old,NULL,16)&&active_phase==PHASE_JOINED&&join_result==0);
 assert(!river_apple80211_link_lost(old.generation,0)&&active_phase==PHASE_JOINED);
 setup();queue_join("one",ap_a);assert(river_apple80211_join_pending(&current));assert(river_apple80211_find_target(&current,&info));
 assert(river_apple80211_join_complete(&current,NULL,16)&&join_result==16);
 assert(river_apple80211_join_complete(&current,&info,0)&&join_result==0&&river_apple80211_data_allowed(current.generation));
 struct apple80211req stop={.req_type=22};assert(!set(&stop));assert(!river_apple80211_join_current(current.generation));
 assert(!river_apple80211_data_allowed(current.generation)&&river_apple80211_take_stop()&&!river_apple80211_take_stop());
 assert(!river_apple80211_join_complete(&current,&info,0)&&active_phase==PHASE_IDLE);
 setup();queue_join("one",ap_b);assert(river_apple80211_join_pending(&current));assert(!river_apple80211_find_target(&current,&info));
 setup();queue_join("one",ap_a);river_connection_service();assert(river_net_live&&active_phase==PHASE_JOINED&&river_wpa.stage==2);
 queued_packets=3;deauth();assert(!river_net_live&&!queued_packets&&river_wpa.stage==3&&river_cleanup_pending);
 assert(!cleanup_calls&&!link_calls);river_connection_service();assert(cleanup_calls==1&&link_calls==1&&!river_cleanup_pending);
 river_connection_service();assert(cleanup_calls==1);river_target.found=true;river_reconnect_wait=0;
 river_connection_service();assert(river_net_live&&river_wpa.stage==2&&active_phase==PHASE_JOINED&&join_result==0&&assoc_calls==2);
 setup();queue_join("one",ap_a);river_connection_service();deauth();assert(!set(&stop));river_connection_service();
 assert(!river_connection_saved&&!river_target.enabled&&!river_net_live&&river_wpa.stage==0&&assoc_calls==1);
 setup();queue_join("one",ap_a);river_connection_service();deauth();struct apple80211req power={.req_type=19,.req_val=0};assert(!set(&power));
 river_connection_service();assert(!river_connection_saved&&!river_target.enabled&&!river_net_live&&active_phase==PHASE_IDLE);
 setup();river_target_bss=11;river_connection_generation=2;river_cleanup_generation=1;river_cleanup_pending=true;river_connection_cleanup();
 assert(!cleanup_calls&&river_target_bss==11&&!river_cleanup_pending);
 setup();queue_join("one",ap_a);inject_new_join=true;river_connection_service();assert(!river_net_live&&join_pending_flag&&active_phase==PHASE_JOINING);
 river_connection_service();assert(river_net_live&&active_phase==PHASE_JOINED&&!memcmp(active_bss.bssid,ap_b,6));
 setup();queue_join("one",ap_a);river_connection_service();deauth();river_connection_cleanup();
 river_target.found=true;river_reconnect_wait=0;river_reconnect_attempts=5;river_connection_service();
 assert(!river_connection_saved&&!river_net_live&&join_result==16&&assoc_calls==1);
 setup();queue_join("one",ap_a);river_connection_service();deauth();river_connection_cleanup();
 river_bss_count=0;river_reconnect_wait=0;river_connection_service();
 assert(river_reconnect_attempts==1&&river_wpa.stage==3&&!river_target.found&&assoc_calls==1);
 river_apple80211_add_bss(ap_a,(const uint8_t*)"one",3,2412,-40,100,0,NULL,0);
 river_reconnect_wait=0;river_connection_service();assert(assoc_calls==2&&river_net_live&&river_wpa.stage==2);
 setup();queue_join("one",ap_a);river_connection_service();deauth();river_connection_cleanup();
 assoc_success=false;river_reconnect_wait=0;river_connection_service();
 assert(assoc_calls==2&&river_wpa.stage==3&&!river_net_live);
 assoc_success=true;river_reconnect_wait=0;river_connection_service();assert(assoc_calls==3&&river_net_live&&river_wpa.stage==2);
 setup();queue_join("one",ap_a);river_connection_service();deauth();river_connection_cleanup();
 wpa_success=false;river_reconnect_wait=0;river_connection_service();
 assert(assoc_calls==2&&river_wpa.stage==3&&!river_net_live);
 wpa_success=true;river_reconnect_wait=0;river_connection_service();assert(assoc_calls==3&&river_net_live&&river_wpa.stage==2);
 setup();queue_join("one",ap_a);inject_stop=true;river_connection_service();
 assert(!river_net_live&&!river_connection_saved&&active_phase==PHASE_IDLE);
 puts("PASS: Wi-Fi generations, stale result rejection, exact BSSID, cancellation, no RX reentrancy, owned deferred cleanup and bounded reconnect");return 0;
}
