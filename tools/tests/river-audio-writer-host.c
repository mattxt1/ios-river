#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#ifndef __unused
#define __unused __attribute__((unused))
#endif
#define LCK_GRP_DECLARE(n,l) int n __attribute__((unused))
#define LCK_MTX_DECLARE(n,g) int n
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static unsigned held,uiomoves,io_fault,hw_writes;
static void lck_mtx_lock(int *p){assert(p);assert(!pthread_mutex_lock(&mutex));held=1;}
static void lck_mtx_unlock(int *p){assert(p && held);held=0;assert(!pthread_mutex_unlock(&mutex));}
static void clock_get_uptime(uint64_t *out){*out=1;}
static void absolutetime_to_nanoseconds(uint64_t t,uint64_t *out){*out=t;}
#include "queue-actual.h"
#define HAKIOS_AUDIO_CHANNELS 2
#define HAKIOS_AUDIO_FRAME 4
#define AUDIO_CHUNK_FRAMES 1024
#define AUDIO_SILENCE 8
#define AUDIO_QUIET_MS 250
#define NSEC_PER_MSEC 1000000
struct uio {char *data;int64_t resid;};
typedef int64_t user_ssize_t;
static int64_t uio_resid(struct uio *u){return u->resid;}
static int uiomove(char *out,int bytes,struct uio *u){assert(!held);uiomoves++;if(io_fault)return EFAULT;memcpy(out,u->data,bytes);u->data+=bytes;u->resid-=bytes;return 0;}
static int river_audio_lock;
static uint64_t audio_frames_total,audio_stretch_last,audio_stretch_first,audio_stretch_frames,audio_sound_frames,audio_stretch_sound;
static unsigned audio_stretch_peak,audio_peak_sample;
static bool audio_report_armed;
static void *audio_report_call;
static uint64_t mach_absolute_time(void){return 1;}
static void clock_interval_to_deadline(unsigned a,unsigned b,uint64_t *out){(void)a;(void)b;*out=1;}
static void thread_call_enter_delayed(void *p,uint64_t t){(void)p;(void)t;assert(0);}
int river_audio_hw_reserve(size_t frames){return ri_audio_queue_reserve(frames);}
void river_audio_hw_cancel(size_t frames,int ticket){if(ticket>0)ri_audio_queue_commit(NULL,frames,true);}
void river_audio_hw_write(const int16_t *pcm,size_t frames,int ticket){assert(!held);hw_writes++;if(ticket>0)ri_audio_queue_commit(pcm,frames,false);else ri_audio_queue_push(pcm,frames);}
#include "writer-actual.h"
static int16_t input[4096],output[2048];
static void reset(void){memset(&ri_audio_queue,0,sizeof(ri_audio_queue));uiomoves=io_fault=hw_writes=0;audio_frames_total=0;assert(!ri_audio_queue_arm(true));}
static void *producer(void *arg){int *result=arg;*result=ri_audio_queue_reserve(1024);if(*result==1)ri_audio_queue_commit(input,1024,false);return NULL;}
int main(void){
 for(unsigned i=0;i<4096;i++)input[i]=1234;
 reset();for(unsigned i=0;i<8;i++){assert(ri_audio_queue_reserve(1024)==1);ri_audio_queue_commit(input,1024,false);}
 struct uio u={(char*)input,4096};assert(river_audio_write(0,&u,0)==EWOULDBLOCK && u.resid==4096 && !uiomoves && !hw_writes && ri_audio_queue.backpressure==1 && !ri_audio_queue.reserved);
 assert(ri_audio_queue_pop(output,0)==1);u=(struct uio){(char*)input,8192};assert(river_audio_write(0,&u,0)==EWOULDBLOCK && u.resid==4096 && uiomoves==1 && hw_writes==1 && ri_audio_queue.used==8192 && !ri_audio_queue.reserved && audio_frames_total==1024);
 reset();io_fault=1;u=(struct uio){(char*)input,4096};assert(river_audio_write(0,&u,0)==EFAULT && u.resid==4096 && uiomoves==1 && !hw_writes && !ri_audio_queue.used && !ri_audio_queue.reserved && ri_audio_queue.cancelled_frames==1024);
 reset();u=(struct uio){(char*)input,1};assert(river_audio_write(0,&u,0)==EINVAL && !uiomoves && !ri_audio_queue.reserved);
 reset();u=(struct uio){(char*)input,8192};assert(!river_audio_write(0,&u,0) && !u.resid && uiomoves==2 && hw_writes==2 && ri_audio_queue.used==2048 && !ri_audio_queue.reserved);
 reset();pthread_t threads[16];int results[16];for(unsigned i=0;i<16;i++)assert(!pthread_create(&threads[i],NULL,producer,&results[i]));unsigned accepted=0;for(unsigned i=0;i<16;i++){assert(!pthread_join(threads[i],NULL));accepted+=results[i]==1;}assert(accepted==8 && ri_audio_queue.used==8192 && !ri_audio_queue.reserved && ri_audio_queue.backpressure==8);
 puts("PASS actual device write: full queue rejects before uiomove, partial writes preserve residual, uiomove failure releases reservation, frame bounds and concurrent reservation capacity");
}
