#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <setjmp.h>
#define LCK_GRP_DECLARE(n,l) int n __attribute__((unused))
#define LCK_MTX_DECLARE(n,g) int n
#define LCK_MTX_ASSERT_OWNED 1
static unsigned queue_held,adsp_held,speaker_held;
static int ri_adsp_lock,ri_speaker_lock;
static void lck_mtx_lock(int *);
static void lck_mtx_unlock(int *);
static uint64_t clock_ns;
static void clock_get_uptime(uint64_t *out){*out=clock_ns;}
static void absolutetime_to_nanoseconds(uint64_t t,uint64_t *ns){*ns=t;}
#include "queue-actual.h"
static void lck_mtx_lock(int *p){
 if(p==&ri_audio_queue_lock){assert(!queue_held);queue_held=1;}
 else if(p==&ri_adsp_lock){assert(!queue_held && !adsp_held && !speaker_held);adsp_held=1;}
 else {assert(p==&ri_speaker_lock && !queue_held && !speaker_held);speaker_held=1;}
}
static void lck_mtx_unlock(int *p){if(p==&ri_audio_queue_lock){assert(queue_held);queue_held=0;}else if(p==&ri_adsp_lock){assert(adsp_held);adsp_held=0;}else{assert(p==&ri_speaker_lock && speaker_held);speaker_held=0;}}
static void lck_mtx_assert(int *p,int flag){assert(p==&ri_adsp_lock && flag==1 && adsp_held);}
typedef uintptr_t vm_offset_t;
typedef int wait_result_t;
typedef void *thread_t;
#define THREAD_NULL NULL
#define KERN_SUCCESS 0
#define THREAD_CONTINUE_NULL NULL
static struct {bool poisoned;} ri_pcm;
static struct {bool complete;} ri_stream;
static struct {bool mapped,in_flight,running,afe_prepared,attempted,complete,eos_rendered;int unmap_error;vm_offset_t page;uint64_t physical;} ri_live_stream;
static struct {bool asm_open,adm_open,afe_started,matrix,muted;int cleanup_error;unsigned failed_opcode;} ri_route;
static struct {bool connected,defined,active,uncertain,attempted,complete;unsigned tid;int cleanup_error;} ri_channel;
static struct {bool active,cutoff,attempted,stopped;int error;} ri_tone;
static struct {bool attempted,complete,restored;} ri_codec_path;
static struct {bool address_valid,enabled,uncertain;} ri_ngd;
static bool ri_afe_mute_hold,ri_i2s_codec_restored,ri_i2s_amp_restored,ri_clock_data_restored;
static unsigned ri_clock_attempted,ri_live_health_index;
static void *ri_tone_timer;
static bool quiet,muted,thread_fail;
static uint32_t ri_i2s_amp_saved[10];
static unsigned cancels,calls,scenario,waits;
static int16_t input[2048];
static jmp_buf exited;
static void (*worker_fn)(void *,wait_result_t);
static bool ri_route_ready(void){return !ri_pcm.poisoned && !ri_route.asm_open && !ri_route.adm_open && !ri_route.afe_started;}
static bool thread_call_cancel_wait(void *p){assert(adsp_held && !queue_held && !speaker_held && p==ri_tone_timer);cancels++;return true;}
static bool ri_i2s_quiet(void){assert(speaker_held && !queue_held);return quiet;}
static bool ri_cs35l41_rd(unsigned reg,uint32_t *out){assert(speaker_held && !queue_held);if(reg==0x2024)*out=muted?0x10:0;else if(reg==0x2018)*out=0x21;else {assert(reg==0x4808);*out=0x20200233;}return true;}
static void ri_wait(unsigned ms){
 assert(!queue_held && !adsp_held && !speaker_held);clock_ns+=ms*1000000ULL;waits++;assert(waits<100);
 if(ms==1 && ri_audio_queue.reserved)ri_audio_queue_commit(NULL,1024,true);
 if(ms==5){if(calls>=2)ri_audio_auto_disable();else for(unsigned i=0;i<4;i++){assert(ri_audio_queue_reserve(1024)==1);ri_audio_queue_commit(input,1024,false);}}
}
static int kernel_thread_start(void (*fn)(void*,wait_result_t),void *arg,thread_t *out){assert(!arg && !queue_held && !adsp_held);if(thread_fail)return 1;worker_fn=fn;*out=&worker_fn;return 0;}
static void thread_deallocate(thread_t t){assert(t==&worker_fn);}
static void *current_thread(void){return &worker_fn;}
static int thread_terminate(void *t){assert(t==&worker_fn && !queue_held && !adsp_held && !speaker_held);return 0;}
static void thread_block(void *p){assert(!p);longjmp(exited,1);}
static int ri_codec_path_run_mode(unsigned mode){
 assert(adsp_held && !queue_held && !speaker_held && mode==4);calls++;
 assert(!ri_codec_path.attempted && !ri_live_stream.attempted && !ri_channel.attempted && !ri_clock_attempted);
 if(calls>1)assert(ri_live_stream.page==0x12340000 && ri_live_stream.physical==0x60000000 && ri_channel.tid==0x77);
 ri_live_stream.page=0x12340000;ri_live_stream.physical=0x60000000;
 ri_codec_path.attempted=ri_codec_path.complete=ri_codec_path.restored=true;
 ri_live_stream.attempted=ri_live_stream.complete=ri_live_stream.eos_rendered=true;
 ri_channel.attempted=ri_channel.complete=true;ri_channel.tid=ri_channel.tid?ri_channel.tid+6:0x77;
 ri_tone.attempted=ri_tone.stopped=true;ri_clock_attempted=1;ri_tone_timer=&worker_fn;
 ri_audio_queue.consumed=2048;ri_audio_queue.used=0;ri_audio_queue.armed=false;
 ri_audio_queue.dsp_submitted=scenario==3?240:2;
 if(scenario==1){ri_pcm.poisoned=true;ri_live_stream.in_flight=ri_live_stream.mapped=true;return ETIMEDOUT;}
 if(scenario==2)ri_i2s_amp_restored=false;
 if(scenario==4)ri_audio_queue.reserved=1024;
 return 0;
}
#include "lifecycle-actual.h"
static void reset(void){
 assert(!queue_held && !adsp_held && !speaker_held);
 memset(&ri_audio_auto,0,sizeof(ri_audio_auto));memset(&ri_audio_queue,0,sizeof(ri_audio_queue));memset(&ri_pcm,0,sizeof(ri_pcm));memset(&ri_live_stream,0,sizeof(ri_live_stream));memset(&ri_route,0,sizeof(ri_route));memset(&ri_channel,0,sizeof(ri_channel));memset(&ri_tone,0,sizeof(ri_tone));memset(&ri_codec_path,0,sizeof(ri_codec_path));
 ri_stream.complete=ri_route.muted=ri_afe_mute_hold=ri_ngd.address_valid=ri_ngd.enabled=ri_i2s_codec_restored=ri_i2s_amp_restored=ri_clock_data_restored=quiet=muted=true;ri_ngd.uncertain=thread_fail=false;ri_clock_attempted=ri_live_health_index=cancels=calls=scenario=waits=0;clock_ns=0;ri_tone_timer=NULL;ri_i2s_amp_saved[0]=0x21;ri_i2s_amp_saved[1]=0x10;ri_i2s_amp_saved[4]=0x20200233;for(unsigned i=0;i<2048;i++)input[i]=1234;
}
int main(void){
 reset();assert(!ri_audio_auto_enable() && ri_audio_auto.enabled && ri_audio_auto.worker && worker_fn);assert(ri_audio_auto_enable()==EBUSY);
 if(!setjmp(exited))worker_fn(NULL,0);
 assert(calls==2 && ri_audio_auto.sessions==2 && ri_audio_auto.restarts==1 && ri_audio_auto.completed_total==4096 && !ri_audio_auto.worker && !ri_audio_auto.enabled && !ri_audio_auto.blocked && cancels==1 && ri_channel.tid==0x7d);
 for(unsigned mode=1;mode<=4;mode++){
  reset();scenario=mode;assert(!ri_audio_auto_enable());if(!setjmp(exited))worker_fn(NULL,0);
  if(mode==1)assert(calls==1 && ri_audio_auto.blocked && ri_pcm.poisoned && ri_live_stream.mapped && ri_live_stream.in_flight && ri_audio_auto.error==ETIMEDOUT && ri_audio_auto_enable()==EBUSY);
  if(mode==2)assert(calls==1 && ri_audio_auto.blocked && ri_audio_auto.error==EIO && ri_audio_auto_enable()==EBUSY);
  if(mode==3)assert(calls==1 && ri_audio_auto.at_limit && !ri_audio_auto.blocked && !ri_audio_auto.enabled);
  if(mode==4)assert(calls==2 && !ri_audio_queue.reserved && ri_audio_queue.cancelled_frames==1024);
 }
 reset();thread_fail=true;assert(ri_audio_auto_enable()==ENOMEM && ri_audio_auto.blocked && !ri_audio_auto.worker);
 /* Every retained-ownership/restoration guard refuses reset without clearing
  * the state that explains the failure. */
 for(unsigned field=0;field<12;field++){
  reset();ri_codec_path.attempted=ri_codec_path.complete=ri_codec_path.restored=ri_live_stream.complete=ri_live_stream.eos_rendered=ri_channel.complete=ri_tone.stopped=true;ri_live_stream.page=0x12340000;
  switch(field){case 0:ri_pcm.poisoned=true;break;case 1:ri_live_stream.mapped=true;break;case 2:ri_live_stream.in_flight=true;break;case 3:ri_route.asm_open=true;break;case 4:ri_route.cleanup_error=EIO;break;case 5:ri_channel.uncertain=true;break;case 6:ri_tone.active=true;break;case 7:ri_tone.cutoff=true;break;case 8:ri_tone.error=EIO;break;case 9:ri_i2s_amp_restored=false;break;case 10:ri_clock_data_restored=false;break;case 11:ri_live_stream.eos_rendered=false;break;}
  lck_mtx_lock(&ri_adsp_lock);assert(ri_audio_auto_rearm()==EBUSY && ri_live_stream.page==0x12340000 && ri_codec_path.attempted);lck_mtx_unlock(&ri_adsp_lock);
 }
 reset();ri_codec_path.attempted=ri_codec_path.complete=ri_codec_path.restored=ri_live_stream.complete=ri_live_stream.eos_rendered=ri_channel.complete=true;muted=false;lck_mtx_lock(&ri_adsp_lock);assert(ri_audio_auto_rearm()==EIO && ri_codec_path.attempted);lck_mtx_unlock(&ri_adsp_lock);
 puts("PASS actual lifecycle worker: native-triggered start, successful stop/rearm/restart, permanent DMA page reuse, monotonic channel IDs, cancellation quiescence, session cap, sticky poison/restore failures, allocation failure and no speculative reset");
}
