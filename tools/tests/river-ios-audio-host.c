#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <setjmp.h>
typedef int wait_result_t;
typedef void *thread_t;
#define THREAD_NULL NULL
#define THREAD_CONTINUE_NULL NULL
#define KERN_SUCCESS 0
#define NSEC_PER_MSEC 1000000
#define THREAD_UNINT 0
#define THREAD_AWAKENED 1
static int ri_adsp_lock,ri_audio_queue_lock,ri_mic_buffer_lock;
static bool adsp_held,queue_held,mic_held;
static void lck_mtx_lock(int *p){
 if(p==&ri_adsp_lock){assert(!adsp_held && !queue_held && !mic_held);adsp_held=true;}
 else if(p==&ri_audio_queue_lock){assert(!queue_held && !mic_held);queue_held=true;}
 else{assert(p==&ri_mic_buffer_lock && !queue_held && !mic_held);mic_held=true;}
}
static void lck_mtx_unlock(int *p){
 if(p==&ri_adsp_lock){assert(adsp_held);adsp_held=false;}
 else if(p==&ri_audio_queue_lock){assert(queue_held);queue_held=false;}
 else{assert(p==&ri_mic_buffer_lock && mic_held);mic_held=false;}
}
static void wakeup(void *p){(void)p;assert(mic_held);}
static struct {bool attempted,enabled,worker,blocked;int error;} ri_audio_normal;
static struct {bool enabled,worker,active,blocked,drain;unsigned reason,sessions;uint64_t last_write;int error;} ri_audio_auto;
static struct {bool enabled,worker,active,blocked,attempted,requested,stop;unsigned reason,gain,sessions;int error;} ri_mic_live;
static struct {bool armed,launched;unsigned used,reserved;} ri_audio_queue;
static struct {bool attempted;} ri_mic_capture;
static struct {bool complete;} ri_stream;
static struct {bool muted;} ri_route;
static struct {bool address_valid,enabled,uncertain;} ri_ngd;
static bool ri_afe_mute_hold,clean,thread_fail;
static unsigned scenario,created,outputs,inputs,waits;
static uint64_t now_ns;
static void (*entry)(void *,wait_result_t);
static bool ri_route_ready(void){return clean;}
static bool ri_mic_owned_clean(void){return clean;}
static bool ri_audio_auto_clean(void){return clean;}
static int ri_audio_auto_rearm(void){assert(adsp_held);return clean?0:EIO;}
static void ri_audio_auto_disable(void){assert(!queue_held && !mic_held);ri_audio_auto.enabled=false;ri_audio_auto.drain=true;ri_audio_queue.armed=false;ri_audio_queue.used=0;}
static uint64_t ri_audio_queue_now(void){return now_ns;}
static int kernel_thread_start(void (*f)(void *,wait_result_t),void *arg,thread_t *t){assert(!adsp_held && !queue_held && !mic_held && !arg);created++;entry=f;*t=(void*)1;return thread_fail?1:0;}
static void thread_deallocate(thread_t t){assert(t==(void*)1);}
static void *current_thread(void){return (void*)2;}
static int thread_terminate(void *t){assert(t==(void*)2);return 0;}
static jmp_buf stopped;
static uint64_t ri_audio_normal_wake_generation;
#define RI_AUDIO_NORMAL_START_FRAMES 1024u
static void ri_audio_startup_mark(unsigned stage){assert(stage==1);}
static bool idle_registered,idle_cancelled,inject_registered;
static unsigned idle_ms,cleared,idle_calls;
static void ri_wait(unsigned ms);
static void clock_interval_to_deadline(unsigned ms,unsigned scale,uint64_t *deadline){assert(scale==NSEC_PER_MSEC && !queue_held && !mic_held && !adsp_held);idle_ms=ms;*deadline=now_ns+ms*1000000ULL;}
static int assert_wait_deadline(void *event,int interruptible,uint64_t deadline){assert(event==&ri_audio_normal && !interruptible && deadline>now_ns);idle_registered=true;idle_cancelled=false;idle_calls++;if(inject_registered)ri_audio_normal_wake_generation++;return 0;}
static int clear_wait(void *thread,int reason){assert(thread==(void*)2 && reason==1 && idle_registered);idle_cancelled=true;cleared++;return 0;}
static void thread_block(void *c){assert(!c && !adsp_held && !queue_held && !mic_held);if(idle_registered){idle_registered=false;if(!idle_cancelled)ri_wait(idle_ms);return;}longjmp(stopped,1);}
static bool ri_audio_normal_input_pending(void);
static void ri_audio_normal_disable(void);
static void ri_audio_auto_session(void){
 assert(!queue_held && !mic_held && !adsp_held && ri_audio_auto.active && !ri_mic_live.active);
 outputs++;ri_audio_auto.sessions++;ri_audio_auto.active=false;ri_audio_queue.armed=false;ri_audio_queue.used=0;
 if(scenario==1){ri_audio_auto.blocked=true;ri_audio_auto.error=EIO;return;}
 if(outputs==30){ri_audio_normal_disable();return;}
 ri_mic_live.requested=true;
}
static void ri_mic_live_session(void){
 assert(!queue_held && !mic_held && !adsp_held && !ri_audio_auto.active && ri_mic_live.active);
 inputs++;ri_mic_live.sessions++;ri_mic_live.active=false;ri_mic_live.stop=true;
 if(scenario==2){ri_mic_live.blocked=true;ri_mic_live.enabled=false;ri_mic_live.error=ETIMEDOUT;return;}
 ri_audio_queue.armed=true;ri_audio_queue.used=1024;
}
static void ri_wait(unsigned ms){assert(!queue_held && !mic_held && !adsp_held);now_ns+=ms*1000000ULL;
 assert(++waits<10);
 if(scenario==3){ri_audio_normal_disable();return;}
 ri_audio_queue.armed=true;ri_audio_queue.used=1024;
}
#include "normal-actual.h"
static void reset(unsigned n){
 memset(&ri_audio_normal,0,sizeof(ri_audio_normal));memset(&ri_audio_auto,0,sizeof(ri_audio_auto));memset(&ri_mic_live,0,sizeof(ri_mic_live));memset(&ri_audio_queue,0,sizeof(ri_audio_queue));memset(&ri_mic_capture,0,sizeof(ri_mic_capture));
 clean=true;ri_stream.complete=ri_route.muted=ri_ngd.address_valid=ri_ngd.enabled=ri_afe_mute_hold=true;ri_ngd.uncertain=false;
 scenario=n;created=outputs=inputs=waits=0;idle_registered=idle_cancelled=inject_registered=false;idle_calls=cleared=0;ri_audio_normal_wake_generation=0;now_ns=0;thread_fail=false;entry=NULL;
}
int main(void){
 for(unsigned n=0;n<4;n++){
  reset(n);assert(!ri_audio_normal_enable());assert(created==1 && entry && ri_audio_normal.worker && ri_audio_auto.worker && ri_mic_live.worker && ri_mic_live.attempted && !ri_mic_live.gain);
  assert(ri_audio_normal_enable()==EBUSY && created==1);
  if(n==0){ri_audio_queue.armed=true;ri_audio_queue.used=1024;ri_mic_live.requested=true;assert(ri_audio_normal_input_pending());}
  if(!setjmp(stopped))entry(NULL,0);
  assert(!ri_audio_normal.worker && !ri_audio_normal.enabled && !ri_audio_auto.worker && !ri_mic_live.worker && !ri_audio_auto.enabled && !ri_mic_live.enabled);
  if(n==0)assert(outputs==30 && inputs==30 && !ri_audio_normal.blocked);
  if(n==1)assert(outputs==1 && !inputs && ri_audio_normal.blocked && ri_audio_normal.error==EIO);
  if(n==2)assert(outputs==1 && inputs==1 && ri_audio_normal.blocked && ri_audio_normal.error==ETIMEDOUT);
  if(n==3)assert(!outputs && !inputs && !ri_audio_normal.blocked);
 }
 reset(4);thread_fail=true;assert(ri_audio_normal_enable()==ENOMEM && ri_audio_normal.blocked && !ri_audio_auto.worker && !ri_mic_live.worker && !ri_audio_normal.worker);
 reset(5);clean=false;assert(ri_audio_normal_enable()==EBUSY && !created && !ri_audio_normal.attempted);
 reset(6);ri_audio_normal_pause(0);assert(idle_calls==1 && idle_ms==1000 && !cleared);
 reset(6);ri_audio_queue.armed=true;ri_audio_queue.used=512;now_ns=40000000;ri_audio_auto.last_write=0;ri_audio_normal_pause(0);assert(idle_ms==110);
 reset(6);ri_audio_normal_wake_generation=1;ri_audio_normal_pause(0);assert(idle_calls==1 && cleared==1 && !waits);
 reset(7);inject_registered=true;ri_audio_normal_pause(0);assert(idle_calls==1 && cleared==1 && !waits);
 puts("PASS actual audio idle wait: idle1000ms fallback, short-tail150ms deadline, work arriving before wait registration cancels sleep, no queue/hardware locks across sleep");
 puts("PASS actual iOS coordinator: one route owner, capture priority, 30 play/record alternations, shared stop, input/output faults sticky, thread failure, prerequisite gate, duplicate rejection, lock boundaries");
}
