#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
typedef void *thread_call_param_t;
typedef void (*timer_fn)(thread_call_param_t,thread_call_param_t);
typedef void *thread_call_t;
#define NSEC_PER_MSEC 1000000
static unsigned ri_speaker_lock,held,writes,fail_write;
#if RIVER_LONG_RUNTIME
static unsigned ri_audio_queue_lock;
static struct {bool long_mode;} ri_audio_queue;
#endif
static bool valid_queue_lock(unsigned *p){
#if RIVER_LONG_RUNTIME
 return p==&ri_audio_queue_lock;
#else
 (void)p;return false;
#endif
}
static bool ri_afe_mute_hold,alloc_fail,queued;
static uint32_t amp[0x11000],clock_fll;
static timer_fn timer;
static void lck_mtx_lock(unsigned *p){assert((p==&ri_speaker_lock || valid_queue_lock(p)) && !held);held=1;}
static void lck_mtx_unlock(unsigned *p){assert((p==&ri_speaker_lock || valid_queue_lock(p)) && held);held=0;}
static bool ri_cs35l41_rd(unsigned reg,uint32_t *out){assert(held && reg<0x11000);*out=amp[reg];return true;}
static bool ri_i2s_amp_write(unsigned reg,uint32_t value){assert(held && (reg==0x2018 || reg==0x2024 || reg==0x6000));writes++;if(writes==fail_write)return false;amp[reg]=value;return true;}
static uint16_t ri_clock_read(unsigned reg){assert(held);switch(reg){case 0x1881:return clock_fll;case 0x700:case 0x708:return 0x8038;case 0x5f7:return 1;case 0x519:return 3;default:assert(0);}return 0;}
static bool ri_i2s_quiet(void){assert(held);return !(amp[0x2018]&1) && (amp[0x2024]&0x10) && !(amp[0x4808]&0x33);}
static thread_call_t thread_call_allocate(timer_fn fn,void *p){assert(held && !p);timer=fn;return alloc_fail?NULL:&timer;}
static void clock_interval_to_deadline(unsigned ms,unsigned scale,uint64_t *deadline){assert(ms>0 && ms<=750 && scale==NSEC_PER_MSEC);*deadline=(uint64_t)ms*1000000ULL;}
static bool thread_call_enter_delayed(thread_call_t call,uint64_t deadline){assert(held && call==&timer && deadline>0 && deadline<=750000000 && !queued);queued=true;return false;}
static bool thread_call_cancel_wait(thread_call_t call){assert(!held && call==&timer);queued=false;return true;}
static void delay(unsigned us){assert(held && us==1000);}
static uint64_t host_now;
static void clock_get_uptime(uint64_t *out){*out=(host_now+=1000);}
static void absolutetime_to_nanoseconds(uint64_t absolute,uint64_t *ns){*ns=absolute;}
#include "tone-actual.h"
static void reset(void){memset(amp,0,sizeof(amp));memset(&ri_tone,0,sizeof(ri_tone));ri_tone_timer=NULL;held=writes=fail_write=0;alloc_fail=queued=false;ri_afe_mute_hold=true;clock_fll=0x100;amp[0x2018]=0x20;amp[0x2024]=0x10;amp[0x6000]=0x80002678;amp[0x4808]=0x20200200;amp[0x4800]=0x10000;amp[0x4c00]=8;amp[0x2014]=1;amp[0x6c04]=0x220;amp[0x10090]=0x40406000;amp[0x10098]=0x2000000a;}
static void stopped(void){assert(!held && !(amp[0x2018]&1) && (amp[0x2024]&0x10) && amp[0x6000]==0x80002678 && !queued);}
int main(void){
 reset();assert(!ri_tone_start() && ri_tone.active && ri_tone.enabled && queued);assert((amp[0x6000]&0x3ff8)==0x3a00 && (amp[0x2018]&1) && !(amp[0x2024]&0x10));assert(!ri_tone_check());ri_tone_stop();assert(ri_tone.stopped && !ri_tone.cutoff && ri_tone.armed_at && ri_tone.unmuted_at>=ri_tone.armed_at && ri_tone.muted_at>=ri_tone.unmuted_at && ri_tone.max_check_ns);stopped();assert(ri_tone_start()==EBUSY);timer(NULL,NULL);stopped();
 reset();assert(!ri_tone_start());timer(NULL,NULL);assert(ri_tone.cutoff && ri_tone_check()==ETIMEDOUT && !ri_tone.active);ri_tone_stop();stopped();
 for(unsigned i=1;i<=3;i++){reset();fail_write=i;assert(ri_tone_start()==EIO);ri_tone_stop();stopped();}
 reset();alloc_fail=true;assert(ri_tone_start()==ENOMEM && !writes);ri_tone_stop();stopped();
 reset();amp[0x4820]=3;assert(ri_tone_start()==EBUSY && !writes);stopped();
 reset();amp[0x4800]=0;assert(ri_tone_start()==EBUSY && !writes);stopped();
 reset();assert(!ri_tone_start());amp[0x4820]=3;assert(ri_tone_check()==EIO);ri_tone_stop();stopped();
 reset();amp[0x4c00]=0x32;assert(ri_tone_start()==EBUSY && !writes);stopped();
 reset();amp[0x10090]|=0x80000000;assert(ri_tone_start()==EBUSY && !writes);stopped();
 reset();clock_fll=0;assert(ri_tone_start()==EBUSY && !writes);stopped();
 reset();assert(!ri_tone_start());amp[0x10098]|=0x10;assert(ri_tone_check()==EIO);ri_tone_stop();stopped();
 reset();assert(!ri_tone_start());amp[0x6000]^=8;assert(ri_tone_check()==EIO);ri_tone_stop();stopped();
 reset();assert(!ri_tone_start());ri_live_health_index=0;
 assert(!ri_live_health() && !ri_live_health() && !ri_live_health());
 amp[0x10090]|=0x80000000;assert(ri_live_health()==EIO && !ri_tone.active && ri_tone.stopped);ri_tone_stop();stopped();
 reset();assert(!ri_tone_start());ri_live_health_index=1;amp[0x10098]|=0x10;assert(ri_live_health()==EIO && !ri_tone.active);ri_tone_stop();stopped();
 reset();assert(!ri_tone_start());ri_live_health_index=2;clock_fll=0xffff;assert(ri_live_health()==EIO && !ri_tone.active);ri_tone_stop();stopped();
 reset();assert(!ri_tone_start());timer(NULL,NULL);assert(ri_live_health()==ETIMEDOUT);ri_tone_stop();stopped();
#if RIVER_LONG_RUNTIME
 reset();ri_audio_queue.long_mode=true;assert(!ri_live_start() && ri_tone.renewable);ri_live_health_index=0;
 uint64_t original_lease=ri_tone.lease_until;
 for(unsigned i=0;i<3;i++){host_now+=20000000ULL;assert(!ri_live_health());if(i<2)assert(!ri_tone.renewals);}
 assert(ri_tone.renewals==1 && ri_tone.lease_until>original_lease);
 host_now=original_lease;queued=false;timer(NULL,NULL);assert(queued && ri_tone.active && !ri_tone.cutoff);
 host_now=ri_tone.lease_until;queued=false;timer(NULL,NULL);assert(!ri_tone.active && ri_tone.cutoff && ri_tone.stopped);ri_tone_stop();stopped();
 reset();ri_audio_queue.long_mode=true;assert(!ri_live_start());ri_live_health_index=0;host_now=ri_tone.lease_until;assert(ri_live_health()==ETIMEDOUT && !ri_tone.renewals && !ri_tone.active);ri_tone_stop();stopped();
 reset();ri_audio_queue.long_mode=true;assert(!ri_live_start());ri_live_health_index=0;host_now+=151000000ULL;assert(ri_live_health()==ETIMEDOUT && !ri_tone.active);ri_tone_stop();stopped();
 reset();ri_audio_queue.long_mode=true;assert(!ri_live_start());ri_live_health_index=0;
 while(host_now+20010000ULL<ri_tone.hard_until){host_now+=20000000ULL;assert(!ri_live_health());}
 assert(ri_tone.renewals>50 && ri_tone.lease_until==ri_tone.hard_until && ri_tone.active);
 host_now=ri_tone.hard_until;queued=false;timer(NULL,NULL);assert(!ri_tone.active && ri_tone.cutoff);ri_tone_stop();stopped();
 puts("PASS renewable watchdog: complete fresh cycles only, stale callback requeue, expired lease cannot revive, slow cycle mute, independent6second hard cutoff");
#endif
 puts("actual-code quiet output attenuation, ownership/clock/fault gates, deadline mute and cleanup tests passed");return 0;
}
