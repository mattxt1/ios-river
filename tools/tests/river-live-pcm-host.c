#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#ifndef EREMOTE
#define EREMOTE 71
#endif
#define LCK_GRP_DECLARE(n,l) int n __attribute__((unused))
#define LCK_MTX_DECLARE(n,g) int n
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static unsigned held;
static void lck_mtx_lock(int *p){assert(p);assert(!pthread_mutex_lock(&mutex));held=1;}
static void lck_mtx_unlock(int *p){assert(p && held);held=0;assert(!pthread_mutex_unlock(&mutex));}
static void clock_get_uptime(uint64_t *);
static void absolutetime_to_nanoseconds(uint64_t,uint64_t *);
#include "queue-actual.h"
static int queue_arm_short(void){
#if RIVER_LONG_RUNTIME
 return ri_audio_queue_arm(false);
#else
 return ri_audio_queue_arm();
#endif
}
#define RI_APR_BAD -1
#define RI_APR_WAIT 0
#define RI_APR_STALE 1
#define RI_APR_ACK_ONLY 2
#define RI_APR_NACK 3
#define RI_APR_STATE 4
#include "decode-actual.h"
typedef uintptr_t vm_offset_t;
#define VM_WIMG_IO 0
struct ri_apr_channel {vm_offset_t info,tx,rx,ipc;};
static uint32_t info[64],tx[2048],rx[2048],ipc[4096];
static vm_offset_t river_smd_smem;
static struct {bool received,state,owned,opened;unsigned info_offset,fifo_offset;} ri_apr;
static struct {bool poisoned;unsigned sequence,transactions,notifications;
 uint32_t rx_header,rx_source,rx_destination,rx_token,rx_opcode;} ri_pcm;
struct ri_stream_state {vm_offset_t page;uint64_t physical;uint32_t handle;bool in_flight;unsigned submitted,completed,bytes_completed;};
static uint64_t now;
static bool ready,resolve_ok,indices_ok,map_fail;
static unsigned maps,unmaps,barriers,health_checks,health_fail,scenario,sends;
static bool ri_apr_dsp_ready(vm_offset_t s){assert(s==river_smd_smem && !held);return ready;}
static bool ri_apr_resolve(vm_offset_t s,struct ri_apr_channel *ch){assert(s==river_smd_smem && !held);*ch=(struct ri_apr_channel){(uintptr_t)info,(uintptr_t)tx,(uintptr_t)rx,0};return resolve_ok;}
static bool ri_apr_indices(const struct ri_apr_channel *ch){assert(ch->info==(uintptr_t)info && !held);return indices_ok;}
static uint32_t ri_rd(vm_offset_t p,unsigned o){assert(!held);return *(uint32_t*)(p+o);}
static void host_barrier(void){assert(!held);barriers++;}
static void ri_apr_write8(vm_offset_t a,unsigned v){assert(!held && v==1 && a>=(uintptr_t)info && a<(uintptr_t)(info+64));}
static void ri_apr_ack_flags(const struct ri_apr_channel *ch){assert(ch->info==(uintptr_t)info && !held);}
static void ri_apr_notify(const struct ri_apr_channel *ch){assert(ch->info==(uintptr_t)info && !held);}
static void clock_get_uptime(uint64_t *t){*t=now;}
static void absolutetime_to_nanoseconds(uint64_t t,uint64_t *ns){*ns=t;}
static void ri_wait(unsigned ms){assert(!held);now+=ms*1000000ULL;}
static vm_offset_t ml_io_map_unmappable(unsigned a,unsigned b,unsigned c){assert(!held && a==0x0b010000 && b==0x4000 && !c);maps++;return map_fail?0:(uintptr_t)ipc;}
static void ml_io_unmap(vm_offset_t p,unsigned n){assert(p==(uintptr_t)ipc && n==0x4000 && !held);unmaps++;}
static void sweet_dcache_clean(vm_offset_t p,unsigned n){assert(p && n==4096 && !held);}
static unsigned append(const uint32_t *packet,unsigned bytes){unsigned h=info[9];rx[h/4]=bytes;for(unsigned i=0;i<bytes;i+=4)rx[((h+20+i)&8191)/4]=packet[i/4];info[9]=(h+20+bytes)&8191;return info[9];}
static void ri_wr(vm_offset_t p,unsigned o,uint32_t v){
 assert(!held);*(uint32_t*)(p+o)=v;
 if(p==(uintptr_t)info && o==16){
  unsigned old=info[3];uint32_t f[18];for(unsigned i=0;i<18;i++)f[i]=tx[((old+i*4)&8191)/4];
  assert(f[0]==52 && f[5]==((52u<<16)|0x250) && f[9]==0x10dab && f[13]==4096);
  info[3]=v;sends++;
  if(scenario==1)return; /* lose every completion */
  uint32_t reply[]={(36u<<16)|0x150,f[7],f[6],f[8],0x10d99,f[10],f[11],f[12],0};
  if(scenario==2)reply[7]^=1;
  if(scenario==3)reply[8]=7;
  append(reply,sizeof(reply));
 }
}
static unsigned output_stops;
static void output_stop(void){assert(!held);output_stops++;}
static void (*ri_stream_output_stop)(void)=output_stop;
#include "pipeline-actual.h"
static int16_t input[2048],output[2048];
static unsigned char dma[16384];
static int health(void){assert(!held);health_checks++;return health_checks==health_fail?EIO:0;}
static void reset(void){memset(&ri_audio_queue,0,sizeof(ri_audio_queue));
#if RIVER_LIFECYCLE
 memset(&ri_audio_auto,0,sizeof(ri_audio_auto));
#endif
 memset(&ri_pcm,0,sizeof(ri_pcm));memset(info,0,sizeof(info));memset(tx,0,sizeof(tx));memset(rx,0,sizeof(rx));ri_apr=(__typeof__(ri_apr)){true,true,true,true,0,0};river_smd_smem=(uintptr_t)info;ri_apr.fifo_offset=(uint32_t)((uintptr_t)tx-river_smd_smem);ready=resolve_ok=indices_ok=true;map_fail=false;now=0;maps=unmaps=barriers=health_checks=health_fail=scenario=sends=0;info[0]=info[5]=2;ri_pipeline_health=health;for(unsigned i=0;i<2048;i++)input[i]=(i&1)?-32768:32767;}
static void fill(unsigned count){for(unsigned i=0;i<count;i++)ri_audio_queue_push(input,1024);}
static void *producer(void *p){(void)p;fill(8);return NULL;}
static void ownership_tests(void){
 reset();struct ri_apr_channel ch;assert(ri_apr_resolve(river_smd_smem,&ch));struct ri_pipeline_slot slots[4]={0};
 for(unsigned i=0;i<4;i++)assert(!ri_pipeline_publish(&ch,&slots[i],0x5010000,0x60000000+i*4096,0xabcd,i));
 assert(ri_pipeline_publish(&ch,&slots[0],0x5010000,0x60000000,0xabcd,0)==EOVERFLOW);
 /* Discard auto replies then return slot3 before slot0, preserving slots1/2. */
 info[8]=info[9];unsigned completed=0;
 uint32_t reply[]={(36u<<16)|0x150,slots[3].request.destination,slots[3].request.source,slots[3].request.token,0x10d99,0x60003000,0,0xabcd,0};
 append(reply,sizeof(reply));assert(!ri_pipeline_receive(&ch,slots,0xabcd,&completed) && completed==1 && !slots[3].owned && slots[0].owned);
 append(reply,sizeof(reply));assert(ri_pipeline_receive(&ch,slots,0xabcd,&completed)==EINPROGRESS && completed==1);
 reply[1]^=1;reply[3]=slots[0].request.token;reply[5]=0x60000000;append(reply,sizeof(reply));assert(ri_pipeline_receive(&ch,slots,0xabcd,&completed)==EINPROGRESS && slots[0].owned);
 reply[1]^=1;reply[7]^=1;append(reply,sizeof(reply));assert(ri_pipeline_receive(&ch,slots,0xabcd,&completed)==EIO && slots[0].owned);
 reply[7]^=1;reply[5]^=4;append(reply,sizeof(reply));assert(ri_pipeline_receive(&ch,slots,0xabcd,&completed)==EIO && slots[0].owned);
 reply[5]^=4;append(reply,sizeof(reply));assert(!ri_pipeline_receive(&ch,slots,0xabcd,&completed) && completed==2 && !slots[0].owned);
 ready=false;assert(ri_pipeline_receive(&ch,slots,0xabcd,&completed)==EIO && slots[1].owned);
}
static unsigned refill_calls;
static int refill_health(void){refill_calls++;
#if RIVER_LONG_RUNTIME
 if(ri_audio_queue.long_mode){for(unsigned i=0;i<4;i++){int ticket=ri_audio_queue_reserve(1024);if(ticket==1)ri_audio_queue_commit(input,1024,false);}return health();}
#endif
 fill(4);return health();}
int main(void){
 reset();ri_audio_queue_push(input,1024);assert(!ri_audio_queue.used);assert(!queue_arm_short() && queue_arm_short()==EALREADY);
 ri_audio_queue_push(NULL,1);ri_audio_queue_push(input,1025);assert(ri_audio_queue.rejected==2);
 fill(9);assert(ri_audio_queue.used==8192 && ri_audio_queue.overflows==1 && ri_audio_queue.dropped==1024);
 for(unsigned i=0;i<8;i++){assert(ri_audio_queue_pop(output,i)==1);assert(output[0]==(i?2048:0) && output[1]==(i?-2048:0));}
 assert(ri_audio_queue_pop(output,8)==-1 && ri_audio_queue.underruns==1);ri_audio_queue.stop=true;assert(!ri_audio_queue_pop(output,8));
 reset();assert(!queue_arm_short());ri_audio_queue_push(input,512);ri_audio_queue_push(input,512);assert(ri_audio_queue_pop(output,0)==1 && ri_audio_queue.consumed==1024);
 fill(8);assert(ri_audio_queue_pop(output,11)==1 && !output[2046] && !output[2047]);
 reset();assert(!queue_arm_short());pthread_t threads[4];for(unsigned i=0;i<4;i++)assert(!pthread_create(&threads[i],NULL,producer,NULL));for(unsigned i=0;i<4;i++)assert(!pthread_join(threads[i],NULL));assert(ri_audio_queue.used==8192 && ri_audio_queue.accepted==8192 && ri_audio_queue.dropped==24*1024);
 ownership_tests();
 for(unsigned mode=0;mode<6;mode++){
  reset();assert(!queue_arm_short());fill(8);scenario=mode;struct ri_stream_state s={.page=(uintptr_t)dma,.physical=0x60000000,.handle=0xabcd};
  if(!mode){ri_pipeline_health=refill_health;refill_calls=0;}
  if(mode==4)health_fail=1;
  if(mode==5)ri_audio_queue.stop=true;
  int error=ri_stream_pipeline(&s,0x5010000);
  if(!mode)assert(!error && sends==12 && s.completed==12 && !s.in_flight && !ri_pcm.poisoned && ri_audio_queue.consumed==12*1024 && refill_calls);
  if(mode==1)assert(error==ETIMEDOUT && s.in_flight && ri_pcm.poisoned);
  if(mode==2)assert(error==EIO && s.in_flight && ri_pcm.poisoned);
  if(mode==3)assert(error==EREMOTE && s.in_flight && ri_pcm.poisoned);
  if(mode==4)assert(error==EIO && !s.in_flight && !ri_pcm.poisoned && s.completed==4 && output_stops);
  if(mode==5)assert(!error && !s.in_flight && !sends && !ri_pcm.poisoned);
  assert(maps==1 && unmaps==1);
 }
 reset();assert(!queue_arm_short());fill(4);struct ri_stream_state s={.page=(uintptr_t)dma,.physical=0x60000000,.handle=0xabcd};assert(ri_stream_pipeline(&s,0x5010000)==ENOBUFS && ri_audio_queue.underruns==1 && !s.in_flight && !ri_pcm.poisoned && s.completed==4);
#if RIVER_LONG_RUNTIME
 reset();assert(!ri_audio_queue_arm(true));
 for(unsigned i=0;i<8;i++)assert(ri_audio_queue_reserve(1024)==1);
 assert(ri_audio_queue.reserved==8192 && ri_audio_queue_reserve(1)==-EWOULDBLOCK && ri_audio_queue.backpressure==1 && !ri_audio_queue.used);
 ri_audio_queue_commit(input,1024,false);assert(ri_audio_queue.used==1024 && ri_audio_queue.reserved==7168);
 ri_audio_queue_commit(NULL,1024,true);assert(ri_audio_queue.reserved==6144 && ri_audio_queue.cancelled_frames==1024);
 ri_audio_queue.stop=true;ri_audio_queue_commit(input,1024,false);assert(ri_audio_queue.reserved==5120 && ri_audio_queue.cancelled_frames==2048);
 reset();assert(!ri_audio_queue_arm(true));for(unsigned i=0;i<8;i++){assert(ri_audio_queue_reserve(1024)==1);ri_audio_queue_commit(input,1024,false);}
 assert(ri_audio_queue_pop(output,239)==1 && !output[2046] && !output[2047]);
 struct ri_stream_state long_s={.page=(uintptr_t)dma,.physical=0x60000000,.handle=0xabcd};ri_pipeline_health=refill_health;
 assert(!ri_stream_pipeline(&long_s,0x5010000) && long_s.completed==240 && !long_s.in_flight && !ri_pcm.poisoned && ri_audio_queue.max_owned==4 && !ri_audio_queue.underruns);
 reset();assert(!ri_audio_queue_arm(true));struct ri_stream_state empty_s={.page=(uintptr_t)dma,.physical=0x60000000,.handle=0xabcd};
 assert(ri_stream_pipeline(&empty_s,0x5010000)==ENOBUFS && now>22000000ULL && ri_audio_queue.underruns==1 && !empty_s.in_flight && !ri_pcm.poisoned);
 puts("PASS runtime reservations, capacity/commit/cancel/stop,240-buffer refill, final ramp and bounded producer-jitter/underrun semantics");
#endif
#if RIVER_LIFECYCLE
 reset();ri_audio_auto.enabled=ri_audio_auto.worker=true;
 assert(ri_audio_queue_reserve(1024)==1 && ri_audio_queue.armed && ri_audio_auto.epoch==1);
 int16_t zero[2048]={0};ri_audio_queue_commit(zero,1024,false);assert(!ri_audio_queue.used && !ri_audio_queue.reserved);
 assert(ri_audio_queue_reserve(512)==1);ri_audio_queue_commit(input,512,false);assert(ri_audio_queue.used==512);
 ri_audio_auto_open();ri_audio_auto_open();ri_audio_auto_close();assert(!ri_audio_auto.drain);ri_audio_auto_close();assert(ri_audio_auto.drain);
 assert(ri_audio_queue_reserve(1024)==-EWOULDBLOCK);
 ri_audio_auto.active=true;assert(ri_audio_queue_pop(output,0)==1 && !output[2046] && !output[2047]);assert(!ri_audio_queue_pop(output,1));
 reset();ri_audio_auto.enabled=ri_audio_auto.worker=true;assert(ri_audio_queue_reserve(1024)==1);ri_audio_auto_disable();ri_audio_queue_commit(input,1024,false);assert(!ri_audio_auto.enabled && !ri_audio_queue.armed && !ri_audio_queue.used && !ri_audio_queue.reserved);
 reset();ri_audio_auto.enabled=ri_audio_auto.worker=true;ri_audio_auto.blocked=true;assert(ri_audio_queue_reserve(1024)==-EIO && !ri_audio_queue.reserved);
 reset();ri_audio_auto.enabled=ri_audio_auto.worker=true;assert(ri_audio_queue_reserve(1024)==1);ri_audio_queue_commit(input,1024,false);ri_audio_auto.active=true;
 for(unsigned i=0;i<24;i++){assert(ri_audio_queue_reserve(1024)==1);ri_audio_queue_commit(zero,1024,false);assert(ri_audio_queue_pop(output,i)==1);}
 assert(ri_audio_auto.drain && ri_audio_auto.reason==2);
 reset();ri_audio_auto.enabled=ri_audio_auto.worker=true;assert(ri_audio_queue_reserve(1024)==1);ri_audio_queue_commit(input,1024,false);ri_audio_auto.active=true;
 struct ri_stream_state idle_s={.page=(uintptr_t)dma,.physical=0x60000000,.handle=0xabcd};
 assert(!ri_stream_pipeline(&idle_s,0x5010000) && idle_s.completed==1 && !idle_s.in_flight && !ri_pcm.poisoned && ri_audio_auto.reason==3 && ri_audio_auto.drain && !ri_audio_queue.underruns);
 puts("PASS lifecycle ingress: first native write arm, silence suppression, last-close drain, partial EOF padding, disable/cancel, sticky poison, sustained silence and idle EOS");
#endif
 puts("PASS actual live queue: overflow, partial chunks, clamp/ramps, concurrent writers; four-slot pipeline: live refill, out-of-order, duplicate, foreign endpoints, address/handle mismatch, timeout, NACK, health failure, stop and underrun ownership");
}
