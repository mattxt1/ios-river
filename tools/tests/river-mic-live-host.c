#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#define HAKIOS_AUDIO_CAPTURE_PHYSICAL_LIVE 32
#define PCATCH 1
static pthread_mutex_t ri_mic_buffer_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ready=PTHREAD_COND_INITIALIZER;
static _Thread_local bool held;
static void lck_mtx_lock(pthread_mutex_t *m){assert(!held);pthread_mutex_lock(m);held=true;}
static void lck_mtx_unlock(pthread_mutex_t *m){assert(held);held=false;pthread_mutex_unlock(m);}
static uint64_t ri_audio_queue_now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000ULL+t.tv_nsec;}
static void wakeup(void *p){assert(p && held);pthread_cond_broadcast(&ready);}
static int sleep_error;
static int msleep(void *p,pthread_mutex_t *m,int flags,const char *name,struct timespec *relative){
 assert(p && held && flags==PCATCH && name && relative);if(sleep_error)return sleep_error;
 struct timespec until;clock_gettime(CLOCK_REALTIME,&until);until.tv_nsec+=relative->tv_nsec;
 if(until.tv_nsec>=1000000000){until.tv_sec++;until.tv_nsec-=1000000000;}
 held=false;int rc=pthread_cond_timedwait(&ready,m,&until);held=true;return rc==ETIMEDOUT?EWOULDBLOCK:rc;
}
#include "mic-live-actual.h"
static void reset(void){memset(&ri_mic_live,0,sizeof(ri_mic_live));sleep_error=0;}
static int16_t pattern(unsigned n){return (int16_t)((n%20001)-10000);}
static void *produce(void *arg){
 unsigned blocks=(unsigned)(uintptr_t)arg;int16_t samples[2048];
 for(unsigned b=0;b<blocks;b++) {
  for(unsigned n=0;n<2048;n++)samples[n]=pattern(b*2048+n);
  for(;;){lck_mtx_lock(&ri_mic_buffer_lock);bool room=ri_mic_live.used+2048<=RI_MIC_RING_FRAMES;lck_mtx_unlock(&ri_mic_buffer_lock);if(room)break;usleep(1000);}
  assert(!ri_mic_live_push(samples,2048,b%3));usleep(1000);
 }
 return NULL;
}
int main(void){
 int16_t samples[512];reset();assert(ri_mic_live_read(samples,512,true)==-ENXIO && !ri_mic_live_flags());
 ri_mic_live.attempted=ri_mic_live.enabled=ri_mic_live.worker=true;ri_mic_live_open();
 assert(ri_mic_live_read(samples,512,true)==-EWOULDBLOCK && ri_mic_live.requested && ri_mic_live.epoch==1 && ri_mic_live_flags()==32);
 ri_mic_live.active=true;ri_mic_live.requested=false;
 pthread_t producer;assert(!pthread_create(&producer,NULL,produce,(void*)(uintptr_t)200));
 unsigned total=0;
 while(total<200*2048){int n=ri_mic_live_read(samples,512,false);assert(n>0 && n<=512);for(int j=0;j<n;j++)assert(samples[j]==pattern(total+(unsigned)j));total+=(unsigned)n;}
 pthread_join(producer,NULL);assert(total==409600 && ri_mic_live.produced==total && ri_mic_live.consumed==total && !ri_mic_live.used && !ri_mic_live.overruns && ri_mic_live.waits && ri_mic_live.high_water<=RI_MIC_RING_FRAMES);
 assert(ri_mic_live.levels[0].frames+ri_mic_live.levels[1].frames+ri_mic_live.levels[2].frames==total);
 ri_mic_live.opens=2;ri_mic_live_close();assert(ri_mic_live.stop && ri_mic_live.reason==1 && !ri_mic_live.used && !ri_mic_live_read(samples,512,false));
 ri_mic_live.active=false;ri_mic_live.sessions=1;ri_mic_live_open();assert(!ri_mic_live.stop);assert(ri_mic_live_read(samples,512,true)==-EWOULDBLOCK && ri_mic_live.epoch==2);
 ri_mic_live.blocked=true;assert(ri_mic_live_read(samples,512,false)==-EIO);ri_mic_live.blocked=false;
 ri_mic_live.reader_busy=true;assert(ri_mic_live_read(samples,512,false)==-EBUSY);ri_mic_live.reader_busy=false;
 ri_mic_live.requested=false;ri_mic_live.active=true;sleep_error=EINTR;assert(ri_mic_live_read(samples,512,false)==-EINTR && ri_mic_live.stop && ri_mic_live.reason==4);sleep_error=0;
 reset();ri_mic_live.attempted=ri_mic_live.enabled=ri_mic_live.active=true;
 int16_t block[2048]={0};block[0]=32767;block[1]=-32768;
 for(unsigned n=0;n<8;n++)assert(!ri_mic_live_push(block,2048,2));
 assert(ri_mic_live_push(block,2048,2)==EOVERFLOW && ri_mic_live.overruns==1 && ri_mic_live.used==16384 && ri_mic_live.levels[2].clipped==16 && ri_mic_live.levels[2].peak==32768);
 ri_mic_live_read_fault(512);assert(ri_mic_live.stop && ri_mic_live.discarded==512 && ri_mic_live.read_faults==1);
 reset();ri_mic_live.enabled=true;ri_mic_live.last_read=ri_audio_queue_now()-251000000ULL;assert(ri_mic_live_stop() && ri_mic_live.reason==3);
 puts("PASS actual continuous mic ring: concurrent 409600-frame delivery beyond snapshot capacity, ordering/no loss, gain/clipping stats, close/reopen, nonblocking/interruptible waits, sticky errors, overrun and read-fault cancellation");
 return 0;
}
