#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#define LCK_GRP_DECLARE(name,label) int name __attribute__((unused))
#define LCK_MTX_DECLARE(name,group) int name
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static void lck_mtx_lock(int *p){assert(p);assert(!pthread_mutex_lock(&mutex));}
static void lck_mtx_unlock(int *p){assert(p);assert(!pthread_mutex_unlock(&mutex));}
#include "actual.h"
static int16_t input[2048],output[2048];
static void *producer(void *unused){(void)unused;ri_native_pcm_push(input,1024);return NULL;}
int main(void){
 memset(input,0,sizeof(input));ri_native_pcm_push(input,1024);assert(!ri_native_pcm.frames && !ri_native_pcm.observed_frames);
 assert(!ri_native_pcm_arm() && ri_native_pcm_arm()==EALREADY && !ri_native_pcm_claim());
 ri_native_pcm_push(NULL,1);ri_native_pcm_push(NULL,1025);assert(ri_native_pcm.rejected==2);
 ri_native_pcm_push(input,1024);assert(ri_native_pcm.observed_frames==1024 && !ri_native_pcm.started && !ri_native_pcm.frames);
 input[2046]=32767;input[2047]=-32768;ri_native_pcm_push(input,1024);assert(ri_native_pcm.started && ri_native_pcm.frames==1 && ri_native_pcm.peak==32768);
 for(unsigned i=0;i<1024;i++){input[i*2]=32767;input[i*2+1]=-32768;}
 for(unsigned i=0;i<4;i++)ri_native_pcm_push(input,1024);
 assert(ri_native_pcm.ready && !ri_native_pcm.armed && ri_native_pcm.frames==4096 && ri_native_pcm_arm()==EALREADY);
 memset(output,0x5a,sizeof(output));assert(!ri_native_pcm_fill(output,0) && output[0]==0x5a5a);assert(ri_native_pcm_claim() && !ri_native_pcm_claim());
 assert(!ri_native_pcm_fill(NULL,0) && !ri_native_pcm_fill(output,4));
 for(unsigned index=0;index<4;index++){
  assert(ri_native_pcm_fill(output,index));for(unsigned i=0;i<1024;i++)assert(output[i*2]==-output[i*2+1] && output[i*2]>=0 && output[i*2]<=2048);
  if(!index)assert(!output[0] && !output[1] && output[480]==2048);
  if(index==3)assert(!output[2046] && !output[2047]);
 }
 struct {unsigned char bytes[sizeof(ri_native_pcm)];} saved;memcpy(saved.bytes,&ri_native_pcm,sizeof(ri_native_pcm));memset(input,0,sizeof(input));ri_native_pcm_push(input,1024);assert(!memcmp(saved.bytes,&ri_native_pcm,sizeof(ri_native_pcm)));
 memset(&ri_native_pcm,0,sizeof(ri_native_pcm));assert(!ri_native_pcm_arm());for(unsigned i=0;i<2048;i++)input[i]=1234;
 pthread_t threads[8];for(unsigned i=0;i<8;i++)assert(!pthread_create(&threads[i],NULL,producer,NULL));for(unsigned i=0;i<8;i++)assert(!pthread_join(threads[i],NULL));
 assert(ri_native_pcm.ready && ri_native_pcm.frames==4096 && ri_native_pcm.observed_frames==4096 && ri_native_pcm.peak==1234 && ri_native_pcm_claim());
 puts("PASS native PCM opt-in, silence trigger, input/frame bounds, immutable freeze/claim, stereo clamp/ramps and concurrent writers");return 0;
}
