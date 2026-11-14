#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
typedef uintptr_t vm_offset_t;
#define VM_WIMG_IO 0
#define LCK_MTX_ASSERT_OWNED 1
#define RI_RPM_REPLY_ACK 1
#define RI_RPM_REQUEST_TIMEOUT (-3)
static uint32_t smem[0x100000/4], aux[0x8000/4], ipc[0x4000/4], rcg[0x4000/4], vote[0x4000/4];
static vm_offset_t river_smd_smem;
static bool ri_adsp_metadata_accepted,ri_adsp_image_staged,ri_adsp_stage_open,ri_adsp_stage_failed;
static bool ri_adsp_handshake_created,ri_adsp_caps_ok,ri_adsp_smem_ok;
static unsigned ri_adsp_channels,ri_adsp_lock,ri_rpm_lock,adsp_owned,rpm_owned;
static struct {bool ready;} ri_rpm_channel;
struct ri_rpm_kv {uint32_t key,value;};
static bool gpu_ready,layout_ok,cap_ok,smem_ok;
static unsigned channel_count,rpm_calls,auth_calls,shutdown_calls,queries,waits,doorbells,maps,unmaps;
static unsigned fail_map,fail_vote,scenario,read_mutation;
static int vote_error,probe_error;
static int64_t query_status,auth_status,shutdown_status;
static uint64_t query_result,auth_result,shutdown_result;
static uint32_t *map_base(uintptr_t address,unsigned bytes)
{
 struct {uint32_t *p;size_t n;} regions[]={{smem,sizeof(smem)},{aux,sizeof(aux)},
     {ipc,sizeof(ipc)},{rcg,sizeof(rcg)},{vote,sizeof(vote)}};
 assert(!(address&3));
 for(unsigned i=0;i<5;i++) {
  uintptr_t start=(uintptr_t)regions[i].p;
  if(address>=start && address-start<=regions[i].n && bytes<=regions[i].n-(address-start))
   return (uint32_t *)address;
 }
 assert(!"out of bounds MMIO");return NULL;
}
static uint32_t ri_rd(vm_offset_t base,unsigned offset)
{
 uint32_t result=*map_base(base+offset,4);
 if(read_mutation && base==(uintptr_t)smem && offset==0x8600c) {
  read_mutation--;smem[0x8600c/4]+=8;
 }
 return result;
}
static void ri_wr(vm_offset_t base,unsigned offset,uint32_t value)
{
 assert(base==(uintptr_t)ipc+0x1008 && offset==0 && value==0x400);
 assert(adsp_owned && !rpm_owned && auth_calls==1);doorbells++;
 *map_base(base+offset,4)=value;
}
static vm_offset_t ml_io_map_unmappable(unsigned pa,unsigned size,unsigned mode)
{
 (void)mode;maps++;
 if(maps==fail_map)return 0;
 switch(pa) {
 case 0x01814000:assert(size==0x4000);return (uintptr_t)rcg;
 case 0x01844000:assert(size==0x4000);return (uintptr_t)vote;
 case 0x60000:assert(size==0x8000);return (uintptr_t)aux;
 case 0x0b010000:assert(size==0x4000);return (uintptr_t)ipc;
 default:assert(!"unexpected map");return 0;
 }
}
static void ml_io_unmap(vm_offset_t base,unsigned bytes)
{ assert(base);(void)bytes;unmaps++; }
static void lck_mtx_assert(unsigned *lock,unsigned owned)
{ assert(lock==&ri_adsp_lock && owned==1 && adsp_owned); }
static void lck_mtx_lock(unsigned *lock)
{ assert(lock==&ri_rpm_lock && !rpm_owned && adsp_owned);rpm_owned=1; }
static void lck_mtx_unlock(unsigned *lock)
{ assert(lock==&ri_rpm_lock && rpm_owned);rpm_owned=0; }
static void host_barrier(void) { }
static bool river_gpu_pas_ready(void) {return gpu_ready;}
static int ri_adsp_probe_impl(void)
{ri_adsp_caps_ok=cap_ok;ri_adsp_smem_ok=smem_ok;ri_adsp_channels=channel_count;return probe_error;}
static bool ri_rpm_layout(vm_offset_t base) {assert(base==(uintptr_t)smem);return layout_ok;}
static int
ri_rpm_request_locked(vm_offset_t base,vm_offset_t doorbell,bool boot,uint32_t set,
    uint32_t type,uint32_t resource,const struct ri_rpm_kv *kv,unsigned count,uint32_t *id)
{
 assert(base==(uintptr_t)aux && doorbell==(uintptr_t)ipc+0x1008 && !boot && rpm_owned);
 unsigned i=rpm_calls++;
 assert(i<4 && set==(i&1));
 if(i<2) {
  assert(type==0x61706d73 && resource==2 && count==3);
  assert(kv[0].key==0x6e657773 && kv[0].value==1);
  assert(kv[1].key==0x6c766c76 && kv[1].value==384);
  assert(kv[2].key==0x616d && kv[2].value==100);
 } else assert(type==0x306b6c63 && resource==0 && count==1 && kv[0].key==0x62616e45 && kv[0].value==1);
 if(i==3 && scenario==6)smem[0x86030/4]=0;
 *id=0x52490003+i;
 return i==fail_vote?vote_error:RI_RPM_REPLY_ACK;
}
static void inbound(uint32_t features,uint32_t state)
{
 const unsigned hdr=0x86188,data=hdr+16;
 smem[hdr/4]=(443u<<16)|0xa5a5;smem[(hdr+4)/4]=344;smem[(hdr+8)/4]=4;
 smem[data/4]=0x504d5324;smem[(data+4)/4]=features;smem[(data+8)/4]=2;
 smem[(data+12)/4]=0x10010;
 const uint32_t name[]={0x76616c73,0x656b2d65,0x6c656e72,0};
 for(unsigned k=0;k<4;k++)smem[(data+20+k*4)/4]=name[k];
 smem[(data+36)/4]=state;smem[0x8600c/4]=0x2f0;
}
static int64_t river_scm(unsigned service,unsigned command,unsigned nargs,
    uint64_t a,uint64_t b,uint64_t c,uint64_t *result)
{
 assert(adsp_owned && !rpm_owned && !b && !c);
 if(service==6) {
  assert(command==1 && nargs==1 && a==0x42000206);
  queries++;*result=query_result;return query_status;
 }
 assert(service==2 && nargs==1 && a==1);
 if(command==5) {
  assert(rpm_calls==4);auth_calls++;*result=auth_result;
  if(scenario==0)inbound(1,2);
  if(scenario==1)inbound(1,3); /* fatal takes precedence over ready */
  if(scenario==3)inbound(0x101,2);
  if(scenario==4)inbound(1,0);
  if(scenario==5) {inbound(1,2);smem[0x86188/4]^=1;}
  if(scenario==8)inbound(0,0);
  return auth_status;
 }
 assert(command==6 && auth_calls==1);shutdown_calls++;*result=shutdown_result;
 return shutdown_status;
}
static void ri_wait(unsigned ms)
{
 assert(ms==20 && adsp_owned && !rpm_owned);waits++;
 if(scenario==3 && waits==3)inbound(1,2); /* remote accepts features0 */
 if(scenario==4 && waits==3)inbound(1,2); /* delayed ready */
 if(scenario==7)inbound(((waits+1)<<8)|1,2); /* changing unsupported offer */
 if(scenario==8 && waits==3)inbound(1,2); /* initialized after publication */
}
static int quiet_printf(const char *format,...) __attribute__((format(printf,1,2)));
static int quiet_printf(const char *format,...){(void)format;return 0;}
#define printf quiet_printf
#include "adsp-start-actual.h"
#undef printf
static void reset(unsigned variant)
{
 memset(smem,0,sizeof(smem));memset(aux,0,sizeof(aux));memset(ipc,0,sizeof(ipc));
 memset(rcg,0,sizeof(rcg));memset(vote,0,sizeof(vote));memset(&ri_adsp_start,0,sizeof(ri_adsp_start));
 river_smd_smem=(uintptr_t)smem;
 ri_adsp_metadata_accepted=ri_adsp_image_staged=ri_adsp_handshake_created=gpu_ready=true;
 ri_adsp_stage_open=ri_adsp_stage_failed=false;
 ri_rpm_channel.ready=layout_ok=cap_ok=smem_ok=true;adsp_owned=1;rpm_owned=0;
 channel_count=rpm_calls=auth_calls=shutdown_calls=queries=waits=doorbells=maps=unmaps=0;
 fail_map=0;fail_vote=99;scenario=variant;read_mutation=0;vote_error=-1;probe_error=0;
 query_status=auth_status=shutdown_status=0;query_result=1;auth_result=shutdown_result=0;
 rcg[0x2008/4]=0x113;vote[0x1004/4]=7;
 smem[0xff000/4]=0x434f5424;smem[0xff004/4]=1;smem[0xff008/4]=7;
 smem[0xff080/4]=0x86000;smem[0xff084/4]=0x24000;smem[0xff08c/4]=0x20000;smem[0xff090/4]=32;
 smem[0x86000/4]=0x54525024;smem[0x86004/4]=0x20000;smem[0x86008/4]=0x24000;
 smem[0x8600c/4]=0x188;smem[0x86010/4]=0x24000;
 smem[0x86020/4]=(429u<<16)|0xa5a5;smem[0x86024/4]=344;smem[0x86028/4]=4;
 smem[0x86030/4]=0x504d5324;smem[0x86034/4]=1;smem[0x86038/4]=0x20000;smem[0x8603c/4]=0x10010;
 const uint32_t name[]={0x7473616d,0x6b2d7265,0x656e7265,0x6c};
 for(unsigned i=0;i<4;i++)smem[(0x86044+i*4)/4]=name[i];
}
static int start(void)
{
 int error=ri_adsp_start_impl();assert(!rpm_owned);
 assert(unmaps==maps-(fail_map && maps>=fail_map?1:0));return error;
}
static void no_start(int expected)
{assert(start()==expected && !rpm_calls && !auth_calls && !shutdown_calls && !doorbells);}
int main(void)
{
 reset(0);assert(start()==0 && ri_adsp_start.ready && rpm_calls==4 && auth_calls==1);
 assert(ri_adsp_start.vote_acks==15 && !shutdown_calls && doorbells==1 && !waits);
 unsigned before=maps;assert(ri_adsp_start_impl()==EALREADY && maps==before && auth_calls==1);
 assert(strstr(ri_adsp_start_note,"phase=ready") && strstr(ri_adsp_start_note,"pcm_transport=0"));
 reset(1);assert(start()==EIO && !ri_adsp_start.ready && shutdown_calls==1 && ri_adsp_start.stopped);
 reset(2);assert(start()==ETIMEDOUT && waits==500 && shutdown_calls==1);
 reset(3);assert(start()==0 && waits==3 && doorbells==2 && ri_adsp_start.remote_fv==1);
 reset(4);assert(start()==0 && waits==3 && doorbells<=4);
 reset(5);assert(start()==EIO && shutdown_calls==1);
 reset(7);assert(start()==ETIMEDOUT && waits==500 && doorbells==4 && shutdown_calls==1);
 reset(8);assert(start()==0 && waits==3 && ri_adsp_start.ready);
 reset(6);assert(start()==EIO && rpm_calls==4 && !auth_calls && !shutdown_calls);
 for(unsigned i=5;i<=6;i++) {
  reset(0);fail_map=i;assert(start()==EBUSY && rpm_calls==4 && !auth_calls);
 }
 reset(0);auth_status=-1;assert(start()==EIO && shutdown_calls==1 && !doorbells);
 reset(0);auth_result=7;shutdown_status=-1;
 assert(start()==EIO && shutdown_calls==1 && !ri_adsp_start.stopped);
 reset(0);auth_result=7;shutdown_result=3;
 assert(start()==EIO && !ri_adsp_start.stopped);
 for(unsigned i=0;i<4;i++)for(unsigned e=0;e<2;e++) {
  reset(0);fail_vote=i;vote_error=e?-3:-1;
  assert(start()==(e?ETIMEDOUT:EIO));
  assert(rpm_calls==i+1 && !auth_calls && !shutdown_calls && !doorbells);
  assert(ri_adsp_start.vote_acks==((1u<<i)-1));
  assert(ri_adsp_start.vote_attempts==((1u<<(i+1))-1));
  before=rpm_calls;assert(ri_adsp_start_impl()==EALREADY && rpm_calls==before);
 }
 for(unsigned i=0;i<6;i++) {
  reset(0);
  switch(i) {case 0:ri_adsp_metadata_accepted=false;break;case 1:ri_adsp_image_staged=false;break;
   case 2:ri_adsp_stage_open=true;break;case 3:ri_adsp_stage_failed=true;break;
   case 4:ri_adsp_handshake_created=false;break;case 5:gpu_ready=false;break;}
  no_start(EBUSY);assert(!maps && !ri_adsp_start.attempted);
 }
 reset(0);channel_count=1;no_start(EBUSY);
 reset(0);probe_error=EIO;no_start(EIO);
 reset(0);cap_ok=false;no_start(EBUSY);
 reset(0);smem_ok=false;no_start(EBUSY);
 reset(0);river_smd_smem=0;no_start(EBUSY);
 reset(0);query_status=-1;no_start(EBUSY);
 reset(0);query_result=0;no_start(EBUSY);
 for(unsigned i=0;i<8;i++) {
  reset(0);
  switch(i) {case 0:rcg[0x2004/4]=1;break;case 1:rcg[0x2008/4]^=1;break;
   case 2:vote[0x1004/4]&=~1u;break;case 3:rcg[0x201c/4]=0x80000000;break;
   case 4:rcg[0x2020/4]=0x80000000;break;case 5:rcg[0x2024/4]=0x80000000;break;
   case 6:fail_map=1;break;case 7:fail_map=2;break;}
  no_start(EBUSY);
 }
 reset(0);fail_map=3;no_start(ENOMEM);
 reset(0);fail_map=4;no_start(ENOMEM);
 reset(0);layout_ok=false;no_start(EBUSY);
 reset(0);ri_rpm_channel.ready=false;no_start(EBUSY);
 reset(0);inbound(1,2);no_start(EBUSY); /* refuse an existing remote */
 struct ri_adsp_remote r;
 const unsigned offsets[]={0xff000,0xff004,0xff008,0xff080,0xff084,0xff088,0xff08c,0xff090,
     0x86000,0x86004,0x86008,0x86020,0x86024,0x86028,0x86030,0x86034,0x86038,0x8603c,0x86044,0x86054};
 for(unsigned i=0;i<sizeof(offsets)/sizeof(offsets[0]);i++) {
  reset(0);smem[offsets[i]/4]^=1;assert(ri_adsp_remote_snapshot((uintptr_t)smem,&r)==EIO);
 }
 reset(0);smem[0x8600c/4]=0x24008;assert(ri_adsp_remote_snapshot((uintptr_t)smem,&r)==EIO);
 reset(0);smem[0x86024/4]=0xfffffff8;assert(ri_adsp_remote_snapshot((uintptr_t)smem,&r)==EIO);
 reset(0);read_mutation=1;assert(ri_adsp_remote_snapshot((uintptr_t)smem,&r)==EAGAIN);
 reset(0);inbound(0,0);assert(ri_adsp_remote_snapshot((uintptr_t)smem,&r)==EAGAIN);
 reset(0);inbound(1,2);smem[(0x86198+12)/4]=0x110010;
 assert(ri_adsp_remote_snapshot((uintptr_t)smem,&r)==EIO);
 reset(0);inbound(1,2);smem[(0x86198+12)/4]=0x20010;
 memcpy(smem+(0x86198+40)/4,smem+(0x86198+20)/4,20);
 assert(ri_adsp_remote_snapshot((uintptr_t)smem,&r)==EIO); /* duplicate slave */
 puts("actual ADSP startup: prerequisite/ownership/clock gates, exact power plan, failures at every vote, PAS auth/shutdown, bounded negotiation/readiness/fatal/timeout and malformed SMEM PASS");
 return 0;
}
