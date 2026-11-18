#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
typedef uintptr_t vm_offset_t;
#define VM_WIMG_IO 0
#define RI_MIC_RING_FRAMES 65536
#define RI_APR_BAD -1
#define RI_APR_STALE 1
#define RI_APR_ACK_ONLY 2
#define RI_APR_NACK 3
#define RI_APR_STATE 4
static unsigned char smem[32768],dma[16384] __attribute__((aligned(16384)));
static vm_offset_t river_smd_smem;
struct ri_apr_channel {vm_offset_t info,tx,rx,ipc;};
struct ri_apr_request {uint32_t source,destination,token,opcode,response;const uint32_t *payload;unsigned words;};
struct ri_apr_reply {uint32_t status,data[32];unsigned words;bool sent,complete;};
static struct {unsigned info_offset,fifo_offset;} ri_apr;
static struct {unsigned sequence,transactions,notifications,rx_header,rx_source,rx_destination,rx_token,rx_opcode;bool poisoned;} ri_pcm;
#include "mic-state-actual.h"
#include "mic-done-actual.h"
#include "mic-decode-actual.h"
static int ri_mic_buffer_lock;
static bool held;
static struct {bool stop;unsigned reason,used,gain,applied_gain;} ri_mic_live;
static unsigned mode,delivered,discarded,invalidations,published,remote_done,max_pending;
static uint64_t now_ns,next_due;
struct pending {uint32_t token,address,sequence;uint64_t due;};
static struct pending pending[8];static unsigned pending_count;
static bool device_owned[4];
static struct ri_apr_channel channel;
static uint32_t ri_rd(vm_offset_t a,unsigned offset){assert(a>=river_smd_smem && a+offset+4<=river_smd_smem+sizeof(smem));uint32_t v;memcpy(&v,(void*)(a+offset),4);return v;}
static void ri_wr(vm_offset_t a,unsigned offset,uint32_t v){assert(a>=river_smd_smem && a+offset+4<=river_smd_smem+sizeof(smem));memcpy((void*)(a+offset),&v,4);}
static void ri_apr_write8(vm_offset_t a,unsigned char v){*(unsigned char*)a=v;}
static void host_barrier(void){}
static bool ri_apr_dsp_ready(vm_offset_t m){assert(m==river_smd_smem);return true;}
static bool ri_apr_resolve(vm_offset_t m,struct ri_apr_channel *c){assert(m==river_smd_smem);*c=channel;return true;}
static bool ri_apr_indices(struct ri_apr_channel *c){return ri_rd(c->info,16)<8192 && ri_rd(c->info,12)<8192 && ri_rd(c->info,36)<8192 && ri_rd(c->info,32)<8192;}
static void ri_apr_ack_flags(struct ri_apr_channel *c){assert(c->info==channel.info);}
static uint64_t ri_pipeline_now(void){return now_ns;}
static void lck_mtx_lock(int *p){assert(p==&ri_mic_buffer_lock && !held);held=true;}
static void lck_mtx_unlock(int *p){assert(p==&ri_mic_buffer_lock && held);held=false;}
static vm_offset_t ml_io_map_unmappable(uint64_t p,unsigned bytes,int type){assert(p==0x0b010000 && bytes==0x4000 && !type);return 0x80000000;}
static void ml_io_unmap(vm_offset_t p,unsigned bytes){assert(p==0x80000000 && bytes==0x4000 && !held);}
static int16_t pattern(unsigned n){return (int16_t)((int)(n%20001)-10000);}
static unsigned frames_per(void){return mode==11?2032:2048;}
static void emit(struct pending p){
 unsigned slot=(p.address-0x60000000)/4096;assert(slot<4 && device_owned[slot]);unsigned frames=frames_per(),offset=mode==11?32:0;
 int16_t *out=(int16_t*)(dma+slot*4096+offset);for(unsigned i=0;i<frames;i++)out[i]=pattern(p.sequence*frames+i);
 uint64_t stamp=(uint64_t)p.sequence*frames*1000000/48000;
 if(mode==5 && p.sequence>=15)stamp+=1000;
 if(mode==7 && p.sequence==15)stamp=0;
 uint32_t packet[19]={(64u<<16)|0x150,0x06010407,0x06010507,p.token,0x10d9a,0,p.address,0,0xabcd,frames*2,offset,(uint32_t)stamp,(uint32_t)(stamp>>32),mode==8?0:0x80000000,1,p.sequence};
 if(p.sequence==0){if(mode==1)packet[6]+=4096;if(mode==2)packet[8]++;if(mode==3)packet[15]++;if(mode==4)packet[13]|=0x40000000;if(mode==12)packet[2]++;}
 unsigned bytes=64;
 if(mode==14){memmove(packet+8,packet+5,11*4);memset(packet+5,0,3*4);bytes=76;packet[0]=(76u<<16)|0x181;}
 if(mode==10 && p.sequence==0){bytes=28;packet[0]=(28u<<16)|0x150;packet[4]=0x110e8;packet[5]=0x10dac;packet[6]=2;}
 uint32_t head=ri_rd(channel.info,36);ri_wr(channel.rx,head,bytes);
 for(unsigned i=0;i<bytes;i+=4)ri_wr(channel.rx,(head+20+i)&8191,packet[i/4]);
 ri_wr(channel.info,36,(head+20+bytes)&8191);device_owned[slot]=false;remote_done++;
}
static void remote_progress(void){
 if(mode==9)return;
 while(pending_count && now_ns>=pending[0].due){
  if(mode==6 && remote_done==0){if(pending_count<2 || now_ns<pending[1].due)return;struct pending a=pending[0],b=pending[1];memmove(pending,pending+2,(pending_count-2)*sizeof(*pending));pending_count-=2;emit(b);emit(a);continue;}
  struct pending p=pending[0];memmove(pending,pending+1,(pending_count-1)*sizeof(*pending));pending_count--;emit(p);
 }
}
static void ri_wait(unsigned ms){assert(!held);now_ns+=ms*1000000ULL;remote_progress();}
static uint16_t ri_clock_read(unsigned reg){assert(!held && (reg==0x1881 || reg==0x311));now_ns+=2000000;remote_progress();return reg==0x311?128:0x100;}
static bool ri_clock_write(unsigned reg,unsigned value){assert(reg==0x311 && value==0x298 && published==4 && remote_done==4 && !pending_count);return true;}
static void ri_mic_live_startup_discard(unsigned n){discarded+=n;}
static void ri_apr_notify(struct ri_apr_channel *c){
 assert(!held && c->info==channel.info);
 uint32_t head=ri_rd(c->info,16),tail=ri_rd(c->info,12);
 while(tail!=head){assert(ri_rd(c->tx,tail)==40);uint32_t token=ri_rd(c->tx,(tail+32)&8191),opcode=ri_rd(c->tx,(tail+36)&8191),address=ri_rd(c->tx,(tail+40)&8191),seq=ri_rd(c->tx,(tail+56)&8191);
  assert(opcode==0x10dac && ri_rd(c->tx,(tail+44)&8191)==0 && ri_rd(c->tx,(tail+48)&8191)==0xabcd && ri_rd(c->tx,(tail+52)&8191)==4096 && seq==published);
  unsigned slot=(address-0x60000000)/4096;assert(slot<4 && !device_owned[slot]);device_owned[slot]=true;
  assert(pending_count<8);if(next_due<now_ns)next_due=now_ns;next_due+=(uint64_t)frames_per()*1000000000ULL/48000;
  pending[pending_count++]=(struct pending){token,address,seq,next_due};if(pending_count>max_pending)max_pending=pending_count;published++;tail=(tail+60)&8191;ri_wr(c->info,12,tail);
 }
}
static bool ri_mic_live_stop(void){if(delivered>=200*frames_per())ri_mic_live.stop=true;return ri_mic_live.stop;}
static int ri_mic_live_push(const int16_t *p,unsigned n,unsigned gain){assert(!held && gain==(mode==13?1u:0u) && n==frames_per());if(ri_mic_live.stop){discarded+=n;return 0;}for(unsigned i=0;i<n;i++)assert(p[i]==pattern(delivered+discarded+i));delivered+=n;return 0;}
static void ri_mic_invalidate_slot(vm_offset_t slot){assert(!held && slot>=(vm_offset_t)dma && slot<(vm_offset_t)dma+16384 && !device_owned[(slot-(vm_offset_t)dma)/4096]);invalidations++;}
#include "mic-pipeline-actual.h"
static void reset(unsigned value){memset(smem,0,sizeof(smem));memset(dma,0,sizeof(dma));memset(&ri_pcm,0,sizeof(ri_pcm));memset(&ri_mic,0,sizeof(ri_mic));memset(&ri_mic_live,0,sizeof(ri_mic_live));memset(device_owned,0,sizeof(device_owned));mode=value;delivered=discarded=invalidations=published=remote_done=max_pending=pending_count=0;now_ns=next_due=0;river_smd_smem=(vm_offset_t)smem;channel=(struct ri_apr_channel){river_smd_smem+256,river_smd_smem+1024,river_smd_smem+12288,0};ri_apr.info_offset=256;ri_apr.fifo_offset=1024;ri_wr(channel.info,0,2);ri_wr(channel.info,20,2);ri_mic.page=(vm_offset_t)dma;ri_mic.physical=0x60000000;ri_mic.handle=0xabcd;ri_mic.gain_level=0;ri_mic_live.gain=value==13?1:0;}
int main(void){
 for(unsigned scenario=0;scenario<=14;scenario++){
  reset(scenario);int error=ri_mic_read_pipeline(0x06010000);fprintf(stderr,"scenario%u err%d poison%u owned%u sub%u done%u max%u pendingmax%u delivered%u pending%u invalid%u gaps%u backwards%u dev%u\n",scenario,error,ri_pcm.poisoned,ri_mic.in_flight,ri_mic.submitted,ri_mic.completed,ri_mic.max_owned,max_pending,delivered,pending_count,invalidations,ri_mic.timestamp_gaps,ri_mic.timestamp_backwards,ri_mic.timestamp_max_deviation_us);
  if(scenario==0 || scenario==5 || scenario==6 || scenario==7 || scenario==8 || scenario==11 || scenario==13 || scenario==14){assert(!error && !ri_pcm.poisoned && !ri_mic.in_flight && ri_mic.submitted==ri_mic.completed && ri_mic.max_owned==4 && max_pending==4 && delivered==200*frames_per() && !pending_count && invalidations==ri_mic.completed );if(scenario==8)assert(ri_mic.timestamp_invalid==ri_mic.completed-(scenario==13?6:4));else assert(ri_mic.timestamp_valid==ri_mic.completed-(scenario==13?6:4));
   if(scenario==5)assert(ri_mic.timestamp_gaps==1);
   else if(scenario==7)assert(ri_mic.timestamp_backwards==1 && ri_mic.timestamp_gaps==1);
   else assert(!ri_mic.timestamp_gaps && !ri_mic.timestamp_backwards && ri_mic.timestamp_max_deviation_us<=1);}

  else{assert(error && ri_mic.in_flight && ri_pcm.poisoned);if(scenario!=12)assert(!invalidations && !delivered);}
 }
 puts("PASS actual queued microphone READs: four owned slots, 200-buffer ordered PCM, reordered completions, partial payloads, slot-only cache invalidation, exact metadata/identity failures retain ownership, no gaps beyond timestamp rounding, timestamp audit, unity startup/gain barrier, settling discard and timeout/NACK retention");
 return 0;
}
