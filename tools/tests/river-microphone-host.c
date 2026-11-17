#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#define LCK_GRP_DECLARE(n,l) int n __attribute__((unused))
#define LCK_MTX_DECLARE(n,g) int n
#define HAKIOS_AUDIO_CAPTURE_SYNTHETIC 2
#define HAKIOS_AUDIO_CAPTURE_PHYSICAL_SNAPSHOT 16
#define PAGE_SIZE 16384
#ifndef EREMOTE
#define EREMOTE 71
#endif
typedef uintptr_t vm_offset_t;
static unsigned held;
static int ri_speaker_lock;
static void lck_mtx_lock(int *p){assert(p && !held);held=(p==&ri_speaker_lock)?2:1;}
static void lck_mtx_unlock(int *p){assert(p && held);held=0;}
#include "mic-buffer-actual.h"
struct ri_apr_request {uint32_t source,destination,token,opcode,response;const uint32_t *payload;unsigned words;};
struct ri_apr_reply {uint32_t status,data[32];unsigned words;bool sent,complete;};
static struct {bool poisoned;uint32_t rx_header,rx_source,rx_destination,rx_token,rx_opcode;} ri_pcm;
static struct {unsigned ack_tid,ack_status;bool acked,ack_waiting;} ri_ngd;
static uint64_t now;
static unsigned mode,reads,invalidations,commands,fail_opcode,transport_calls,codec_writes,fail_write;
static uint16_t regs[0x2000],initial[0x2000];
static uint32_t mute;
static bool route_touched;
static unsigned char dma[PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));
static uint64_t ri_pipeline_now(void){return now;}
static void ri_wait(unsigned ms){assert(held!=1);now+=ms*1000000ULL;}
static uint16_t ri_clock_read(uint16_t reg){assert(held!=1 && reg<0x2000);return regs[reg];}
static bool ri_clock_write(uint16_t reg,uint16_t value){assert(held==2 && reg<0x2000);codec_writes++;if(codec_writes==fail_write)return false;regs[reg]=value;if(reg==0x300)regs[0x301]=value;return true;}
static bool ri_cs35l41_rd(unsigned reg,uint32_t *out){assert(held==2 && reg==0x2024);*out=mute;return true;}
static int ri_pcm_transaction(struct ri_apr_request *r,struct ri_apr_reply *reply);
static int ri_ngd_send(vm_offset_t base,uint32_t *packet,unsigned bytes){assert(base==0x1000 && !held && bytes<=19);assert(((uint8_t*)packet)[1]==0x2c && ((uint8_t*)packet)[3]==0x8a && ((uint8_t*)packet)[4]==8 && ((uint8_t*)packet)[5]==128);if(mode==20)return ETIMEDOUT;ri_ngd.acked=true;return 0;}
static int ri_ngd_poll(vm_offset_t base,bool *sent){assert(base==0x1000 && !held);*sent=false;return 0;}
#include "mic-protocol-actual.h"
static void ri_mic_invalidate(vm_offset_t page){assert(!held && page==(uintptr_t)dma && !ri_mic.in_flight);invalidations++;}
static int ri_pcm_transaction(struct ri_apr_request *r,struct ri_apr_reply *reply){
 assert(!held && !ri_pcm.poisoned);commands++;memset(reply,0,sizeof(*reply));reply->sent=reply->complete=true;
 if(r->opcode==fail_opcode)return EREMOTE;
 const uint32_t stream=0x06010000;
 switch(r->opcode){
 case 0x100ef:{uint32_t expected[]={(36u<<16)|0x4001,0,0,0,0x1020c,0x10212,24,1,16u<<16,1u<<16,128,0,48000};assert(r->source==0x504 && r->destination==0x404 && r->words==13 && !memcmp(expected,r->payload,sizeof(expected)));break;}
 case 0x100f0:{uint32_t expected[]={(24u<<16)|0x4001,0,0,0,0x1020c,0x10212};assert(r->source==0x504 && r->destination==0x404 && r->response==0x10106 && r->words==6 && !memcmp(expected,r->payload,sizeof(expected)));
  uint32_t response[]={0,0x1020c,0x10212,24,1,16u<<16,1u<<16,128,0,48000};reply->words=10;memcpy(reply->data,response,sizeof(response));
  if(mode==40){reply->words=12;reply->data[10]=0xdead;reply->data[11]=0xbeef;}
  if(mode==41)reply->words=9;
  if(mode>=42 && mode<=51)reply->data[mode-42]^=1;
  break;
 }
 case 0x10db4:{uint32_t expected[]={0,0,0x10c68,0x10da5,16};assert(r->source==(stream|0x507) && r->destination==(stream|0x407) && r->words==5 && !memcmp(expected,r->payload,sizeof(expected)));break;}
 case 0x10c10:{uint32_t expected[]={0x10da3,28,0,20,(16u<<16)|1,48000,1,3,0};assert(r->words==9 && !memcmp(expected,r->payload,sizeof(expected)));break;}
 case 0x10326:assert(r->source==0x40010508 && r->destination==0x40010408 && r->words==7 && r->payload[0]==(2u<<16) && r->payload[1]==0xffff4001 && r->payload[5]==3);reply->words=2;reply->data[1]=7;
 if(mode==60){reply->data[0]=2;reply->data[1]=0xffff;}
 if(mode==61)reply->words=1;
 if(mode==62)reply->data[1]=0x10007;
 if(mode==63)reply->data[1]=0xff;
 if(mode==64)reply->data[1]=0xf0b70009;
 break;
 case 0x10325:assert(r->words==4 && r->payload[0]==1 && r->payload[1]==1 && r->payload[2]==0x10006 && r->payload[3]==(mode==64?9u:7u));break;
 case 0x10daa:assert(r->words==3);break;
 case 0x10dac:{
  assert(r->source==(stream|0x507) && r->destination==(stream|0x407) && r->response==0x10d9a && r->words==5 && r->payload[0]==0x60000000+(reads&3)*4096 && !r->payload[1] && r->payload[2]==0xabcd && r->payload[3]==4096 && r->payload[4]==reads);
  int16_t *samples=(int16_t*)(dma+(reads&3)*4096);for(unsigned i=0;i<2048;i++)samples[i]=(int16_t)((i&1)?-1234:4321);
  reply->words=11;reply->data[1]=r->payload[0];reply->data[3]=0xabcd;reply->data[4]=4096;reply->data[8]=0x80000000;reply->data[9]=1;reply->data[10]=reads;
  reads++;now+=42666667ULL;
  switch(mode){case 1:reply->words=10;break;case 2:reply->data[0]=7;break;case 3:reply->data[1]+=4096;break;case 4:reply->data[2]=1;break;case 5:reply->data[3]++;break;case 6:reply->data[4]=4097;break;case 7:reply->data[5]=2;break;case 8:reply->data[4]=4095;break;case 9:reply->data[8]|=0x40000000;break;case 10:reply->data[10]++;break;case 11:reply->data[4]=0;break;case 12:reply->data[8]|=1;break;case 13:now+=4000000000ULL;break;}
  break;
 }
 case 0x10bd3:case 0x10bcd:assert(r->source==(stream|0x507) && r->destination==(stream|0x407));break;
 case 0x10327:assert(r->source==0x40010508 && r->destination==(mode==64?0x00090408u:0x00070408u));break;
 case 0x100e6:assert(r->source==0x504 && r->destination==0x404 && r->words==1 && r->payload[0]==0x4001);break;
 case 0x10d94:assert(!ri_mic.in_flight && !ri_mic.asm_open && !ri_mic.adm_open && !ri_mic.afe_started && r->payload[0]==0xabcd);break;
 default:assert(0);
 }
 return 0;
}
static int ri_mic_transport(void);
#include "mic-codec-actual.h"
static int ri_mic_transport(void){
 assert(!held && (regs[0x101]&0x40) && regs[0x300]==2 && (regs[0x311]&255)==128 && !(regs[0x311]&0x100) && regs[0x5f6]==1 && (regs[0x7c0]&255)==0x10 && (regs[0x219]&0x81eb)==0x8169 && (regs[0x21e]&7)==1 && (regs[0x213]&0x7e0)==0x440 && !(regs[0x200]&6) && (regs[0x200]&1) && mute==0x10);
 transport_calls++;route_touched=true;return mode==30?EIO:0;
}
static void reset(void){assert(!held);memset(&ri_mic,0,sizeof(ri_mic));memset(&ri_mic_capture,0,sizeof(ri_mic_capture));memset(&ri_ngd,0,sizeof(ri_ngd));memset(regs,0,sizeof(regs));ri_pcm.poisoned=false;mode=reads=invalidations=commands=fail_opcode=transport_calls=codec_writes=fail_write=0;now=0;mute=0x10;route_touched=false;regs[0x1881]=0x100;ri_mic.page=(uintptr_t)dma;ri_mic.physical=0x60000000;ri_mic.handle=0xabcd;}
struct uio {char *data;int64_t resid;};
typedef int64_t user_ssize_t;
static unsigned user_moves;
static int river_audio_lock;
static uint64_t mic_frames_read;
static unsigned mic_phase;
static int64_t uio_resid(struct uio *u){return u->resid;}
static int uiomove(char *from,int bytes,struct uio *u){assert(!held);memcpy(u->data,from,bytes);u->data+=bytes;u->resid-=bytes;user_moves++;return 0;}
int river_mic_hw_read(int16_t *samples,unsigned frames){return ri_mic_buffer_read(samples,frames);}
#include "mic-reader-actual.h"
int main(void){
 reset();assert(!ri_mic_afe_config() && ri_mic.afe_words==10 && ri_mic.afe_reply[7]==128);
 reset();mode=40;assert(!ri_mic_afe_config() && ri_mic.afe_words==12);
 for(unsigned n=41;n<=51;n++){reset();mode=n;assert(ri_mic_afe_config()==EIO && commands==2 && !reads && !invalidations && !ri_mic.afe_started);}
 reset();fail_opcode=0x100ef;assert(ri_mic_afe_config()==EREMOTE && commands==1);
 reset();fail_opcode=0x100f0;assert(ri_mic_afe_config()==EREMOTE && commands==2);
 puts("PASS actual AFE TX SET/GET: nonzero24-byte capacity, exact self-declared record, bounded opaque tail, mismatch/rejection stops before start");
 int16_t samples[512];reset();assert(ri_mic_buffer_read(samples,512)==-ENXIO && ri_mic_buffer_flags()==2);ri_mic_capture.attempted=true;assert(ri_mic_buffer_read(samples,512)==-EWOULDBLOCK);ri_mic_capture.done=true;assert(ri_mic_buffer_read(samples,512)==-EIO);
 reset();ri_mic.mapped=ri_mic.afe_started=true;assert(!ri_mic_dsp_run() && reads==48 && ri_mic.frames==98304 && ri_mic.peak==4321 && invalidations==48 && !ri_mic.in_flight);
 ri_mic_capture.attempted=ri_mic_capture.done=ri_mic_capture.complete=true;ri_mic_capture.frames=ri_mic.frames;assert(ri_mic_buffer_flags()==16);
 for(unsigned i=0;i<192;i++){assert(ri_mic_buffer_read(samples,512)==512);assert(samples[0]==4321 && samples[1]==-1234);}assert(!ri_mic_buffer_read(samples,512));
 ri_mic_dsp_cleanup();assert(!ri_mic.mapped && !ri_mic.asm_open && !ri_mic.adm_open && !ri_mic.afe_started && !ri_mic.running && !ri_mic.cleanup_error);
 reset();mode=60;ri_mic.mapped=ri_mic.afe_started=true;assert(ri_mic_dsp_run()==EREMOTE && !ri_pcm.poisoned && !ri_mic.adm_open && ri_mic.failed_opcode==0x10326 && ri_mic.failed_status==2 && ri_mic.adm_words==2 && ri_mic.adm_reply[1]==0xffff);
 ri_mic_dsp_cleanup();assert(!ri_mic.mapped && !ri_mic.asm_open && !ri_mic.afe_started && !ri_mic.cleanup_error);
 reset();mode=64;ri_mic.mapped=ri_mic.afe_started=true;assert(!ri_mic_dsp_run() && ri_mic.copp==9 && ri_mic.adm_reply[1]==0xf0b70009 && reads==48);ri_mic_dsp_cleanup();assert(!ri_mic.mapped && !ri_mic.asm_open && !ri_mic.adm_open && !ri_mic.afe_started && !ri_mic.cleanup_error);
 reset();mode=62;assert(!ri_mic_dsp_run() && ri_mic.copp==7 && ri_mic.adm_reply[1]==0x10007);ri_mic_dsp_cleanup();
 for(unsigned n=61;n<=63;n++){if(n==62)continue;reset();mode=n;ri_mic.mapped=ri_mic.afe_started=true;assert(ri_mic_dsp_run()==EIO && ri_pcm.poisoned && !reads && ri_mic.failed_opcode==0x10326);unsigned before=commands;ri_mic_dsp_cleanup();assert(commands==before && ri_mic.mapped);}
 puts("PASS actual ADM TX typed rejection preserves exact status and permits cleanup; 16-bit ID ignores reserved upper bits and routes/closes only masked ID; truncated/out-of-range success replies retain ownership");
 for(unsigned n=1;n<=12;n++){reset();mode=n;ri_mic.mapped=ri_mic.afe_started=true;assert(ri_mic_dsp_run()==EIO && ri_pcm.poisoned && ri_mic.in_flight && !invalidations && !ri_mic.frames);unsigned before=commands;ri_mic_dsp_cleanup();assert(commands==before && ri_mic.mapped);}
 reset();mode=13;assert(ri_mic_dsp_run()==ETIMEDOUT && reads==1 && !ri_pcm.poisoned && !ri_mic.in_flight);
 for(unsigned op=0;op<5;op++){reset();uint32_t ops[]={0x10db4,0x10c10,0x10326,0x10325,0x10daa};fail_opcode=ops[op];ri_mic.mapped=ri_mic.afe_started=true;assert(ri_mic_dsp_run()==EREMOTE);fail_opcode=0;ri_mic_dsp_cleanup();assert(!ri_mic.mapped && !ri_mic.asm_open && !ri_mic.adm_open && !ri_mic.afe_started);}
 reset();uint8_t connect[]={0x8a,8,128,0};ri_mic.tid=0x91;assert(!ri_mic_channel_command(0x1000,0x2c,connect,4,3) && connect[3]==0x92);mode=20;assert(ri_mic_channel_command(0x1000,0x2c,connect,4,3)==ETIMEDOUT && ri_mic.uncertain);
 /* Every failed codec write must take the restoration path. No amp output
  * write function even exists in this harness. */
 reset();memcpy(initial,regs,sizeof(regs));lck_mtx_lock(&ri_speaker_lock);assert(!ri_mic_codec());lck_mtx_unlock(&ri_speaker_lock);unsigned total=codec_writes;assert(transport_calls==1 && ri_mic.codec_restored && !memcmp(initial,regs,sizeof(regs)));
 for(unsigned n=1;n<=total;n++){reset();memcpy(initial,regs,sizeof(regs));fail_write=n;lck_mtx_lock(&ri_speaker_lock);int error=ri_mic_codec();lck_mtx_unlock(&ri_speaker_lock);assert(!memcmp(initial,regs,sizeof(regs)) || !ri_mic.codec_restored);assert(error==EIO || !error);}
 reset();mode=30;memcpy(initial,regs,sizeof(regs));lck_mtx_lock(&ri_speaker_lock);assert(ri_mic_codec()==EIO);lck_mtx_unlock(&ri_speaker_lock);assert(ri_mic.codec_restored && !memcmp(initial,regs,sizeof(regs)));
 reset();regs[0x300]=2;lck_mtx_lock(&ri_speaker_lock);assert(ri_mic_codec()==EBUSY && !codec_writes);lck_mtx_unlock(&ri_speaker_lock);
 reset();user_moves=0;struct uio user={(char*)samples,sizeof(samples)};ri_mic_capture.attempted=true;
 assert(river_mic_read(0,&user,0)==EWOULDBLOCK && user.resid==sizeof(samples) && !user_moves);
 ri_mic_capture.done=true;assert(river_mic_read(0,&user,0)==EIO && !user_moves);
 ri_mic_capture.complete=true;ri_mic_capture.frames=512;for(unsigned n=0;n<512;n++)ri_mic_capture.samples[n]=(int16_t)(n+100);
 assert(!river_mic_read(0,&user,0) && !user.resid && user_moves==1 && samples[0]==100 && samples[511]==611);
 user=(struct uio){(char*)samples,sizeof(samples)};assert(!river_mic_read(0,&user,0) && user.resid==sizeof(samples) && user_moves==1);
 reset();mic_phase=0;user=(struct uio){(char*)samples,sizeof(samples)};assert(!river_mic_read(0,&user,0) && !user.resid && samples[0]==-47 && !commands);
 puts("PASS actual cdev mic read: pending/failure never return synthetic data; completed physical samples and EOF; untouched default stays tagged synthetic");
 puts("PASS actual physical mic: READ command/11-word exact completion, all identity/range/flags/sequence failures retain ownership before cache/sample access; stash never falls back after physical request; capture/cleanup/timeout, channel ACK uncertainty, source-supported codec/bias/supply config and failed-write rollback");
}
