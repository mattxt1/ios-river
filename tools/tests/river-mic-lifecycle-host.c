#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <setjmp.h>
typedef uintptr_t vm_offset_t;typedef void *thread_t;typedef int wait_result_t;
#define THREAD_NULL NULL
#define KERN_SUCCESS 0
#define THREAD_CONTINUE_NULL NULL
#define LCK_MTX_ASSERT_OWNED 1
static int ri_adsp_lock,ri_mic_buffer_lock,ri_speaker_lock,ri_codec_lock,ri_audio_queue_lock;
static bool adsp,buffer,speaker,codec,queue,muted=true;
static void lck_mtx_lock(int *p){
 if(p==&ri_adsp_lock){assert(!adsp && !buffer && !speaker && !queue);adsp=true;}
 else if(p==&ri_mic_buffer_lock){assert(!buffer);buffer=true;}
 else if(p==&ri_speaker_lock){assert(!speaker && !buffer);speaker=true;}
 else if(p==&ri_codec_lock){assert(!codec && !buffer);codec=true;}
 else {assert(p==&ri_audio_queue_lock && !queue);queue=true;}
}
static void lck_mtx_unlock(int *p){if(p==&ri_adsp_lock){assert(adsp);adsp=false;}else if(p==&ri_mic_buffer_lock){assert(buffer);buffer=false;}else if(p==&ri_speaker_lock){assert(speaker);speaker=false;}else if(p==&ri_codec_lock){assert(codec);codec=false;}else{assert(queue);queue=false;}}
static void lck_mtx_assert(int *p,int flag){assert(p==&ri_adsp_lock && adsp && flag==1);}
static struct {bool poisoned;} ri_pcm;
static struct {bool mapped,in_flight,running,asm_open,adm_open,afe_started,connected,defined,active,uncertain,complete,codec_restored,attempted,live;int cleanup_error,error;vm_offset_t page;uint64_t physical;unsigned tid,gain_level,submitted,completed,failed_opcode,failed_status,adm_reply[2];const char *phase;} ri_mic;
static struct {bool enabled,worker,active,requested,reader_busy,blocked,stop,attempted;unsigned gain,sessions;int error;char note[2048];} ri_mic_live;
static struct {bool enabled,worker,active;} ri_audio_auto;
static struct {bool attempted;} ri_mic_capture;
static struct {bool complete;} ri_stream;
static struct {bool muted;} ri_route;
static struct {bool address_valid,enabled,uncertain;} ri_ngd;
static bool ri_afe_mute_hold;
static unsigned ri_clock_attempted,scenario,runs,waits;
static int (*ri_clock_capture_test)(void);
static char ri_clock_note[64];
static jmp_buf exited;
static bool ri_cs35l41_rd(unsigned reg,uint32_t *v){assert(adsp && speaker && reg==0x2024);*v=muted?16:0;return true;}
static bool ri_route_ready(void){return !ri_pcm.poisoned;}
static int ri_audio_auto_rearm(void){assert(adsp && !queue);return scenario==9?EBUSY:0;}
static void wakeup(void *p){assert(p==&ri_mic_live && buffer);}
static int ri_mic_codec(void){assert(adsp && codec && speaker);return 0;}
static int ri_clock_run_impl(bool test,bool route,bool i2s){
 assert(adsp && codec && speaker && test && route && !i2s && ri_clock_capture_test==ri_mic_codec && !ri_clock_attempted);
 runs++;ri_clock_attempted=1;ri_mic.page=0x10000;ri_mic.physical=0x20000000;ri_mic.tid+=6;ri_mic.submitted=ri_mic.completed=200;ri_mic.codec_restored=true;ri_mic.phase="captured";strcpy(ri_clock_note,"clock_restored=1 pins_restored=1");
 if(scenario==1){ri_pcm.poisoned=true;ri_mic.mapped=true;ri_mic.uncertain=true;return EIO;}
 if(scenario==2){ri_mic.codec_restored=false;return EIO;}
 return 0;
}
static void ri_wait(unsigned ms){
 assert(!adsp && !buffer && !speaker && !codec && !queue && ms==5 && ++waits<10);
 if(runs==1 && !scenario){assert(ri_mic.complete && ri_mic.page==0x10000);ri_mic_live.requested=true;ri_mic_live.stop=false;ri_mic_live.gain=2;}
 else ri_mic_live.enabled=false;
}
static void *current_thread(void){return (void*)1;}
static int thread_terminate(void *t){assert(t==(void*)1);return 0;}
static void thread_block(void *c){assert(!c && !adsp && !buffer && !codec && !speaker && !queue);longjmp(exited,1);}
static bool thread_fail;
static int kernel_thread_start(void (*f)(void*,wait_result_t),void *arg,thread_t *t){assert(f && !arg && !adsp && !buffer);*t=(void*)2;return thread_fail?1:0;}
static void thread_deallocate(thread_t t){assert(t==(void*)2);}
#include "mic-lifecycle-actual.h"
static void reset(void){assert(!adsp && !buffer && !codec && !speaker && !queue);memset(&ri_mic,0,sizeof(ri_mic));memset(&ri_mic_live,0,sizeof(ri_mic_live));memset(&ri_audio_auto,0,sizeof(ri_audio_auto));ri_pcm.poisoned=false;ri_stream.complete=ri_route.muted=ri_afe_mute_hold=ri_ngd.address_valid=ri_ngd.enabled=true;ri_ngd.uncertain=ri_mic_capture.attempted=false;ri_clock_attempted=scenario=runs=waits=0;muted=true;thread_fail=false;}
int main(void){
 reset();assert(!ri_mic_live_enable() && ri_mic_live.worker && ri_mic_live.enabled);assert(ri_mic_live_enable()==EALREADY);
 ri_mic_live.requested=true;if(!setjmp(exited))ri_mic_live_worker(NULL,0);
 assert(runs==2 && ri_mic_live.sessions==2 && !ri_mic_live.worker && !ri_mic_live.enabled && !ri_mic_live.active && !ri_mic_live.blocked && ri_mic.gain_level==2 && ri_mic.complete && ri_mic.tid==12 && ri_mic.page==0x10000);
 lck_mtx_lock(&ri_adsp_lock);assert(!ri_mic_live_release() && !ri_clock_attempted);lck_mtx_unlock(&ri_adsp_lock);
 for(unsigned mode=1;mode<=2;mode++){reset();scenario=mode;assert(!ri_mic_live_enable());ri_mic_live.requested=true;if(!setjmp(exited))ri_mic_live_worker(NULL,0);assert(runs==1 && ri_mic_live.blocked && !ri_mic_live.worker && ri_mic_live.error==EIO);lck_mtx_lock(&ri_adsp_lock);assert(ri_mic_live_release()==EBUSY);lck_mtx_unlock(&ri_adsp_lock);}
 reset();lck_mtx_lock(&ri_adsp_lock);ri_mic.page=0x10000;ri_mic.physical=0x20000000;ri_mic.tid=0x97;ri_mic.attempted=ri_mic.complete=ri_mic.codec_restored=true;ri_clock_attempted=1;assert(!ri_mic_live_rearm() && ri_mic.page==0x10000 && ri_mic.physical==0x20000000 && ri_mic.tid==0x97 && !ri_clock_attempted);lck_mtx_unlock(&ri_adsp_lock);
 bool *owned[]={&ri_mic.mapped,&ri_mic.in_flight,&ri_mic.running,&ri_mic.asm_open,&ri_mic.adm_open,&ri_mic.afe_started,&ri_mic.connected,&ri_mic.defined,&ri_mic.active,&ri_mic.uncertain};
 for(unsigned i=0;i<sizeof(owned)/sizeof(*owned);i++){reset();*owned[i]=true;lck_mtx_lock(&ri_adsp_lock);assert(ri_mic_live_rearm()==EBUSY);lck_mtx_unlock(&ri_adsp_lock);}
 reset();ri_mic.tid=0xea;lck_mtx_lock(&ri_adsp_lock);assert(ri_mic_live_rearm()==EOVERFLOW);lck_mtx_unlock(&ri_adsp_lock);
 reset();ri_audio_auto.enabled=true;assert(ri_mic_live_enable()==EBUSY && !ri_mic_live.attempted);
 reset();thread_fail=true;assert(ri_mic_live_enable()==ENOMEM && ri_mic_live.blocked && !ri_mic_live.worker);
 puts("PASS actual mic lifecycle: reader-driven restart, permanent DMA/monotonic IDs, all ownership guards, clock restoration, poisoned/stuck-state retention, speaker exclusion and thread-start failure");
 return 0;
}
