#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
struct ri_apr_request {uint32_t source,destination,token,opcode,response;const uint32_t *payload;unsigned words;};
struct ri_apr_reply {uint32_t status,data[32];unsigned words;bool sent,complete;};
static struct {bool unmapped,asm_closed,poisoned;uint64_t physical;unsigned transactions;uint32_t rx_header,rx_source,rx_destination,rx_token,rx_opcode;} ri_pcm;
static bool ri_afe_mute_hold,ri_speaker_ok;
static unsigned ri_audio_queue_lock;
static unsigned ri_speaker_lock,lock_depth,commands,mode;
static uint32_t mute_control,fail_opcode,fail_status,cleanup_fail_opcode,ops[128],digital;
static bool timeout_failure,mute_failure,afe_config_live;
static void lck_mtx_lock(unsigned *p) {assert((p==&ri_speaker_lock || p==&ri_audio_queue_lock) && !lock_depth);lock_depth++;}
static void lck_mtx_unlock(unsigned *p) {assert((p==&ri_speaker_lock || p==&ri_audio_queue_lock) && lock_depth==1);lock_depth--;}
static bool ri_cs35l41_rd(uint32_t reg,uint32_t *out) {assert(reg==0x2024 || reg==0x6000);*out=reg==0x2024?mute_control:digital;return !mute_failure;}
static bool ri_cs35l41_wr(uint32_t reg,uint32_t value) {assert(reg==0x2024 || reg==0x6000);if(reg==0x2024)mute_control=value;else digital=value;return !mute_failure;}
typedef uintptr_t vm_offset_t;
typedef uint64_t ppnum_t;
typedef uint64_t addr64_t;
#define PAGE_SIZE 16384u
#define PAGE_MASK (PAGE_SIZE-1)
#define KMA_DATA 1
#define KMA_ZERO 2
#define KMA_PERMANENT 4
#define VM_KERN_MEMORY_IOKIT 1
#define KERN_SUCCESS 0
static int kernel_map,kernel_pmap;
static unsigned write_count,cleaned,barriers,allocations,output_starts,output_stops;
static bool output_live,expected_tone,eos_seen,expected_native,expected_queued,expected_long;
static uint32_t expected_pa,expected_stream=0x04010000;
static unsigned char stream_page[PAGE_SIZE*2] __attribute__((aligned(PAGE_SIZE)));
static int kmem_alloc_contig(int map,vm_offset_t *out,unsigned size,unsigned mask,unsigned low,unsigned high,unsigned flags,unsigned tag)
{assert(!map && size==PAGE_SIZE && mask==PAGE_MASK && !low && !high && flags==7 && tag==1);if(mode==41)return ENOMEM;assert(allocations<2);*out=(uintptr_t)stream_page+allocations++*PAGE_SIZE;memset((void*)*out,0xa5,PAGE_SIZE);expected_pa=0x60000000+(allocations-1)*PAGE_SIZE;return 0;}
static ppnum_t pmap_find_phys(int map,addr64_t address)
{assert(!map && address>=(uintptr_t)stream_page && address<(uintptr_t)stream_page+2*PAGE_SIZE);return mode==42?0:0x60000000/PAGE_SIZE+(address-(uintptr_t)stream_page)/PAGE_SIZE;}
#define ptoa_64(n) ((n)*PAGE_SIZE)
static void sweet_dcache_clean(vm_offset_t address,unsigned size)
{assert(address>=(uintptr_t)stream_page && address<(uintptr_t)stream_page+2*PAGE_SIZE && (size==PAGE_SIZE || size==4096));if(size==PAGE_SIZE){for(unsigned i=0;i<size;i++)assert(!((unsigned char*)address)[i]);}
 else {assert(size==4096 && output_live);int16_t *v=(int16_t*)address;unsigned nonzero=0;for(unsigned i=0;i<1024;i++){assert((expected_native?v[i*2]==-v[i*2+1]:v[i*2]==v[i*2+1]) && v[i*2]<=2048 && v[i*2]>=-2048);nonzero+=v[i*2]!=0;}assert(nonzero);if(write_count==4)assert(v[0]==0 && v[1]==0);if(write_count==7)assert(v[2046]==0 && v[2047]==0);}
 cleaned++;}
static void host_barrier(void){barriers++;}
static int ri_pcm_transaction(struct ri_apr_request *r,struct ri_apr_reply *p)
{
 memset(p,0,sizeof(*p));assert(!lock_depth && commands<128 && !ri_pcm.poisoned);
 ops[commands++]=r->opcode;ri_pcm.transactions++;p->sent=true;
 if(r->opcode==fail_opcode || r->opcode==cleanup_fail_opcode) {
  p->status=fail_status;
  if(timeout_failure){ri_pcm.poisoned=true;return ETIMEDOUT;}
  p->complete=true;return EREMOTE;
 }
 p->complete=true;
 switch(r->opcode) {
 case 0x10d92:
  assert(r->source==0x507 && r->destination==0x407 && r->response==0x10d93 && r->words==5);
  assert(r->payload[0]==0x10003 && !r->payload[1] && r->payload[2]==expected_pa && !r->payload[3] && r->payload[4]==PAGE_SIZE);
  assert(cleaned>=1 && barriers);p->words=1;p->data[0]=mode==44?0:0xaabbccdd;break;
 case 0x10d94:
  assert(r->source==0x507 && r->destination==0x407 && r->words==1 && r->payload[0]==0xaabbccdd);break;
 case 0x10daa:
  assert(r->source==(expected_stream|0x507) && r->destination==(expected_stream|0x407) && r->words==3);
  assert(!r->payload[0] && !r->payload[1] && !r->payload[2]);break;
 case 0x10bdb:assert(expected_tone && output_live && write_count==(expected_queued?(expected_long?244u:16u):8u) && r->response==0x10c1c && !r->words);p->words=mode==57?1:0;eos_seen=true;break;
 case 0x10bd3:assert(!r->words);break;
 case 0x10dab: {
  assert(r->source==(expected_stream|0x507) && r->destination==(expected_stream|0x407) && r->response==0x10d99 && r->words==8);
  assert(write_count<(expected_tone?8u:4u) && r->payload[0]==expected_pa+(write_count&3)*4096 && !r->payload[1]);
  assert(r->payload[2]==0xaabbccdd && r->payload[3]==4096 && r->payload[4]==write_count && !r->payload[5] && !r->payload[6] && !r->payload[7]);
  write_count++;p->words=4;p->data[0]=r->payload[0];p->data[1]=0;p->data[2]=r->payload[2];p->data[3]=0;
  if(mode==30)p->data[0]^=32;
  if(mode==31)p->data[2]++;
  if(mode==32)p->words=3;
  if(mode==33)p->data[3]=7;
  break;
 }
 case 0x100ef: {
  uint32_t expected[]={(36u<<16)|0x4000,0,0,0,0x1020c,0x10212,24,1,16u<<16,1u<<16,144,0,48000};
  assert(r->source==0x504 && r->destination==0x404 && r->words==13 && !r->response);
  assert(!memcmp(r->payload,expected,sizeof(expected)));afe_config_live=true;break;
 }
 case 0x100f0:
  assert(r->words==6 && r->payload[0]==((24u<<16)|0x4000) && r->response==0x10106);
  p->data[0]=2;p->words=1;
  if(mode==2 || mode==3 || mode>=6) {
   uint32_t data[]={0,0x1020c,0x10212,24,1,16u<<16,1u<<16,144,0,48000};
   memcpy(p->data,data,sizeof(data));p->words=10;
   if(mode==3)p->data[9]++;
   if(mode==6) {p->words=13;p->data[10]=0xabcdef01;p->data[11]=0x12345678;}
   if(mode==7)p->data[3]=36;
   if(mode==8)p->data[3]|=1u<<16;
   if(mode==9)p->words=9;
   if(mode==10)p->data[1]++;
   if(mode==11)p->data[2]++;
   if(mode==12)p->data[4]++;

  }
  break;
 case 0x100e5:case 0x100e6:
  assert(r->source==0x504 && r->destination==0x404 && r->words==1 && r->payload[0]==0x4000);
  assert(ri_afe_mute_hold && (mute_control&0x10));
  if(r->opcode==0x100e5 && !afe_config_live) {p->status=2;return EREMOTE;}
  if(r->opcode==0x100e6)afe_config_live=false;
  break;
 case 0x10db3: {
  uint32_t session=(r->source>>24)&0xff;
  assert(session==2 || session==3 || session==4 || session==5);assert(r->source==(((session<<8)|1)<<16|0x507));
  assert(r->destination==(((session<<8)|1)<<16|0x407));
  uint32_t data[]={0,16u<<16,0x10c68,0x10da5};assert(r->words==4 && !memcmp(r->payload,data,sizeof(data)));break;
 }
 case 0x10d98:assert(r->words==6 && r->payload[2]==48000);break;
 case 0x10bcd:assert(!r->words);break;
 case 0x10326: {
  uint32_t data[]={1u<<16,0xffff4000,0x10312,(16u<<16)|1,48000,3,0};
  assert(r->source==0x40000508 && r->destination==0x40000408 && r->response==0x10329);
  assert(r->words==7 && !memcmp(r->payload,data,sizeof(data)));
  p->words=2;p->data[1]=mode==4?0xff:0; /* COPP0 is valid. */
  if(mode==5)p->data[0]=2;
  break;
 }
 case 0x10325:
  assert(r->source==0x508 && r->destination==0x408 && r->words==4);
  assert(r->payload[0]==0 && r->payload[1]==1 && (r->payload[2]==((1u<<16)|2) || r->payload[2]==((1u<<16)|3) || r->payload[2]==((1u<<16)|4) || r->payload[2]==((1u<<16)|5)) && r->payload[3]==0);break;
 case 0x10327:assert(r->source==0x40000508 && r->destination==0x408 && !r->words);break;
 default:assert(0);
 }
 return 0;
}
static bool ri_native_pcm_fill(int16_t *target,unsigned index){
 assert(expected_native && index<4);if(mode==60)return false;
 for(unsigned i=0;i<1024;i++){unsigned frame=index*1024+i,remaining=4095-frame,ramp=frame<240?frame:remaining<240?remaining:240;target[i*2]=(int16_t)(2048*(int)ramp/240);target[i*2+1]=-target[i*2];}return true;
}
static int quiet_printf(const char *format,...) {(void)format;return 0;}
#define printf quiet_printf
#include "route-actual.h"
#undef printf
#define printf quiet_printf
static struct {bool stop,armed,long_mode,waiting_output,go;unsigned used,discarded_at_stop;} ri_audio_queue;
#if RIVER_LONG_RUNTIME
static uint64_t host_gate_now;
static uint64_t ri_pipeline_now(void){return host_gate_now;}
static void ri_wait(unsigned ms){assert(!lock_depth);host_gate_now+=(uint64_t)ms*1000000ULL;if(mode==63)ri_audio_queue.go=true;if(mode==64)ri_audio_queue.stop=true;}
#endif
#include "stream-actual.h"
/* Model pipeline return values here; full transport/queue is exercised by
 * test-river-live-pcm.py. This harness checks route/EOS/cleanup integration. */
static int ri_stream_pipeline(struct ri_stream_state *state,uint32_t stream) {
 assert(output_live && stream==expected_stream && state->completed==4);
 if(mode==61) {state->in_flight=true;ri_pcm.poisoned=true;return ETIMEDOUT;}
 if(mode==62)return ENOBUFS;
 unsigned n=expected_long?240:12;state->submitted+=n;state->completed+=n;state->bytes_completed+=n*4096;write_count+=n;return 0;
}
#undef printf
static void reset(unsigned scenario)
{
 memset(&ri_pcm,0,sizeof(ri_pcm));memset(&ri_route,0,sizeof(ri_route));
 ri_pcm.unmapped=ri_pcm.asm_closed=ri_speaker_ok=true;
 ri_afe_mute_hold=false;afe_config_live=false;commands=lock_depth=0;mute_control=0xa5000001;digital=0;
 mode=scenario;fail_opcode=cleanup_fail_opcode=0;fail_status=7;timeout_failure=mute_failure=false;
}
int route_baseline_main(void)
{
 reset(1);assert(ri_route_config()==0 && ri_route.configured && !ri_route.config_readback && ri_route.get_status==2);
 assert(ri_route_config()==EALREADY);assert(ri_route_test(false)==0);
 assert(!ri_route.asm_open && !ri_route.adm_open && !ri_route.matrix && !ri_afe_mute_hold);
 assert(ri_route_test(true)==0);assert(ri_afe_mute_hold && (mute_control&0x10));
 assert(!ri_route.afe_started && !ri_route.asm_open && !ri_route.adm_open);
 assert(ri_route_test(true)==EALREADY && ri_route_test(false)==EALREADY);
 reset(2);assert(ri_route_config()==0 && ri_route.config_readback);
 reset(6);assert(ri_route_config()==0 && ri_route.config_readback && ri_route.get_words==13 && ri_route.get_bytes==24);
 for(unsigned m=7;m<=12;m++) {reset(m);assert(ri_route_config()==EIO && !ri_route.configured);}
 reset(3);assert(ri_route_config()==EIO && !ri_route.configured);assert(ri_route_test(false)==EBUSY);
 reset(1);fail_opcode=0x100ef;assert(ri_route_config()==EREMOTE && !ri_route.configured && commands==1);
 const uint32_t failures[]={0x10db3,0x10d98,0x10326,0x10325};
 for(unsigned i=0;i<4;i++) {
  reset(1);assert(ri_route_config()==0);fail_opcode=failures[i];
  assert(ri_route_test(false)==EREMOTE);assert(!ri_route.asm_open && !ri_route.adm_open);
  assert(ri_route.failed_opcode==fail_opcode && ri_route.failed_status==7);
 }
 reset(4);assert(ri_route_config()==0);assert(ri_route_test(false)==EIO && ri_pcm.poisoned && ri_route.asm_open);
 reset(5);assert(ri_route_config()==0);assert(ri_route_test(false)==EREMOTE && !ri_route.asm_open && !ri_route.adm_open);
 reset(1);assert(ri_route_config()==0);fail_opcode=0x10325;timeout_failure=true;
 assert(ri_route_test(false)==ETIMEDOUT && ri_pcm.poisoned && ri_route.asm_open && ri_route.adm_open);
 reset(1);assert(ri_route_config()==0);cleanup_fail_opcode=0x10327;
 assert(ri_route_test(false)==EREMOTE && ri_route.adm_open && !ri_route.asm_open);
 assert(ri_route.cleanup_error==EREMOTE && ri_route.cleanup_opcode==0x10327);
 assert(ri_route_test(true)==EBUSY);
 reset(1);assert(ri_route_config()==0);fail_opcode=0x100e5;
 assert(ri_route_test(true)==EREMOTE && !ri_route.afe_started && !ri_route.asm_open && ri_afe_mute_hold);
 reset(1);assert(ri_route_config()==0);cleanup_fail_opcode=0x100e6;
 assert(ri_route_test(true)==EREMOTE && ri_route.afe_started && ri_afe_mute_hold);
 reset(1);assert(ri_route_config()==0);mute_failure=true;unsigned before=commands;
 assert(ri_route_test(true)==EIO && commands==before && !ri_route.afe_started);
 reset(1);ri_pcm.unmapped=false;assert(ri_route_config()==EBUSY && !commands);
 puts("actual-code AFE configuration, ADM routing, muted start/stop and teardown failure tests passed");
 return 0;
}

static int host_output_start(void){assert(!lock_depth && write_count==4);output_starts++;if(mode==56)return EIO;output_live=true;return 0;}
static int host_output_check(void){assert(!lock_depth && output_live && !eos_seen);return ((mode==58 && write_count>=7) || (mode==59 && write_count>=6))?ETIMEDOUT:0;}
static void host_output_stop(void){assert(!lock_depth);output_stops++;output_live=false;}
static void stream_reset(unsigned scenario)
{
 reset(scenario);output_starts=output_stops=0;output_live=expected_tone=eos_seen=expected_native=expected_queued=expected_long=false;memset(&ri_audio_queue,0,sizeof(ri_audio_queue));ri_stream_output_start=NULL;ri_stream_output_check=NULL;ri_stream_output_stop=NULL;allocations=0;expected_stream=0x04010000;memset(&ri_live_stream,0,sizeof(ri_live_stream));memset(&ri_stream,0,sizeof(ri_stream));
 ri_pcm.physical=0x50000000;write_count=cleaned=barriers=0;
 assert(ri_route_config()==0 && ri_route.config_readback);
 assert(ri_route_test(false)==0);
 assert(!ri_route.hot_attempted && ri_route.cold_attempted);
 commands=0;ri_pcm.transactions=0;
}
int main(void)
{
 route_baseline_main();
 stream_reset(2);assert(afe_config_live);assert(ri_stream_test()==0);
 assert(ops[0]==0x10d92 && ops[1]==0x100ef && ops[2]==0x100f0 && ops[3]==0x100e5);
 assert(ri_stream.complete && ri_stream.completed==4 && ri_stream.submitted==4 && ri_stream.bytes_completed==16384);
 assert(!ri_stream.mapped && !ri_stream.in_flight && !ri_stream.running && !ri_route.afe_started && !ri_route.asm_open && !ri_route.adm_open);
 assert(ri_afe_mute_hold && (mute_control&0x10) && cleaned==1 && ri_stream.page);
 assert(ri_stream_test()==EALREADY);
 for(unsigned scenario=30;scenario<=32;scenario++) {
  stream_reset(scenario);assert(ri_stream_test()==EIO && ri_pcm.poisoned);
  assert(ri_stream.page && ri_stream.mapped && ri_stream.in_flight && ri_route.asm_open && ri_route.adm_open && ri_route.afe_started);
 }
 stream_reset(33);assert(ri_stream_test()==EREMOTE && !ri_pcm.poisoned && !ri_stream.in_flight && !ri_stream.mapped && !ri_stream.completed);
 stream_reset(41);assert(ri_stream_test()==ENOMEM && !commands);
 stream_reset(42);assert(ri_stream_test()==EIO && !commands && ri_stream.page);
 stream_reset(2);ri_pcm.physical=0x60000000;assert(ri_stream_test()==EIO && !commands);
 stream_reset(44);assert(ri_stream_test()==EIO && ri_pcm.poisoned && !ri_route.afe_started && ri_stream.page);
 stream_reset(2);fail_opcode=0x10dab;assert(ri_stream_test()==EREMOTE);
 assert(!ri_pcm.poisoned && ri_stream.in_flight && ri_stream.mapped && !ri_route.asm_open && !ri_route.adm_open && !ri_route.afe_started);
 stream_reset(2);fail_opcode=0x10dab;timeout_failure=true;assert(ri_stream_test()==ETIMEDOUT && ri_pcm.poisoned && ri_stream.in_flight);
 stream_reset(2);fail_opcode=0x10daa;assert(ri_stream_test()==EREMOTE && !ri_stream.mapped && !ri_route.afe_started);
 stream_reset(2);afe_config_live=false;assert(ri_stream_test()==0 && ri_stream.complete);
 stream_reset(2);ri_route.cold_attempted=false;assert(ri_stream_test()==EBUSY && !ri_stream.attempted);
 stream_reset(2);fail_opcode=0x100e5;assert(ri_stream_test()==EREMOTE);
 assert(ri_stream.failed_opcode==0x100e5 && ri_stream.failed_status==7 && !ri_stream.mapped && !ri_route.afe_started && !ri_stream.submitted);
 stream_reset(2);fail_opcode=0x100ef;assert(ri_stream_test()==EREMOTE && !ri_stream.mapped && !ri_route.afe_started && !ri_route.configured);
 stream_reset(2);fail_opcode=0x100f0;assert(ri_stream_test()==EREMOTE && !ri_stream.mapped && !ri_route.config_readback && !ri_route.afe_started);
 stream_reset(2);mode=1;assert(ri_stream_test()==EIO && !ri_stream.mapped && !ri_route.afe_started);
 const uint32_t cleanup_failures[]={0x10bd3,0x10bcd,0x10327,0x100e6,0x10d94};
 for(unsigned i=0;i<5;i++) {
  stream_reset(2);cleanup_fail_opcode=cleanup_failures[i];assert(ri_stream_test()==EREMOTE);
  assert(ri_stream.mapped && !ri_stream.complete && ri_stream.page && ri_afe_mute_hold);
 }
 stream_reset(2);ri_route.failed_opcode=0x100e5;assert(ri_stream_test()==EBUSY && !ri_stream.attempted);
 stream_reset(2);ri_route.config_readback=false;assert(ri_stream_test()==EBUSY && !ri_stream.attempted);
 /* Fresh session5 owns separate permanent memory and preserves prior evidence. */
 stream_reset(2);assert(!ri_stream_test());struct ri_stream_state prior=ri_stream;
 commands=write_count=0;expected_stream=0x05010000;
 assert(!ri_stream_execute(&ri_live_stream,5));assert(ri_live_stream.complete && ri_live_stream.session==5 && ri_live_stream.completed==4);
 assert(ri_live_stream.physical!=ri_stream.physical && !memcmp(&prior,&ri_stream,sizeof(prior)));
 assert(!ri_live_stream.mapped && !ri_route.afe_started && !ri_route.asm_open && !ri_route.adm_open);
 assert(ri_stream_execute(&ri_live_stream,5)==EALREADY);
 stream_reset(2);assert(!ri_stream_test());prior=ri_stream;commands=write_count=0;expected_stream=0x05010000;
 fail_opcode=0x10dab;timeout_failure=true;
 assert(ri_stream_execute(&ri_live_stream,5)==ETIMEDOUT && ri_live_stream.mapped && ri_live_stream.in_flight && ri_pcm.poisoned);
 assert(!memcmp(&prior,&ri_stream,sizeof(prior)));
 /* AFE START precedes sink setup and is not repeated by the engine. */
 stream_reset(2);assert(!ri_stream_test());commands=write_count=0;expected_stream=0x05010000;
 assert(!ri_stream_prepare_live() && ri_stream_live_prepared() && commands==3);
 assert(ops[0]==0x100ef && ops[1]==0x100f0 && ops[2]==0x100e5);
 assert(ri_stream_prepare_live()==EALREADY);unsigned before=commands;
 assert(!ri_stream_execute(&ri_live_stream,5) && ri_live_stream.complete && !ri_live_stream.afe_prepared);
 for(unsigned i=before;i<commands;i++)assert(ops[i]!=0x100e5 && ops[i]!=0x100ef);
 stream_reset(2);assert(!ri_stream_test());commands=write_count=0;fail_opcode=0x100e5;
 assert(ri_stream_prepare_live()==EREMOTE && !ri_live_stream.afe_prepared && !ri_route.afe_started);
 assert(ri_live_stream.failed_opcode==0x100e5 && ri_stream_prepare_live()==EALREADY);
 stream_reset(2);assert(!ri_stream_test());commands=write_count=0;assert(!ri_stream_prepare_live());
 ri_stream_cleanup(&ri_live_stream);assert(!ri_route.afe_started && !ri_live_stream.afe_prepared);
 /* Tone output is armed only after4 zero completions; EOS drain precedes mute. */
 for(unsigned scenario=2;scenario<=59;scenario++) {
  if(scenario!=2 && scenario!=56 && scenario!=57 && scenario!=58 && scenario!=59)continue;
  stream_reset(2);assert(!ri_stream_test());commands=write_count=0;expected_stream=0x05010000;
  assert(!ri_stream_prepare_live());ri_live_stream.tone=true;expected_tone=true;mode=scenario;
  ri_stream_output_start=host_output_start;ri_stream_output_check=host_output_check;ri_stream_output_stop=host_output_stop;
  int rc=ri_stream_execute(&ri_live_stream,5);assert(output_starts==1 && output_stops==1 && !output_live);
  if(scenario==2)assert(!rc && ri_live_stream.complete && ri_live_stream.eos_rendered && ri_live_stream.completed==8 && ri_live_stream.bytes_completed==32768);
  else if(scenario==56)assert(rc==EIO && write_count==4 && !ri_live_stream.mapped);
  else if(scenario==57)assert(rc==EIO && ri_pcm.poisoned && ri_live_stream.mapped);
  else assert(rc==ETIMEDOUT && !ri_live_stream.mapped && !ri_pcm.poisoned);
 }
 for(unsigned scenario=2;scenario<=60;scenario+=58) {
  stream_reset(2);assert(!ri_stream_test());commands=write_count=0;expected_stream=0x05010000;
  assert(!ri_stream_prepare_live());ri_live_stream.tone=ri_live_stream.native_pcm=true;expected_tone=expected_native=true;mode=scenario;
  ri_stream_output_start=host_output_start;ri_stream_output_check=host_output_check;ri_stream_output_stop=host_output_stop;
  int rc=ri_stream_execute(&ri_live_stream,5);
  assert(output_starts==1 && output_stops==1 && !output_live && !ri_live_stream.mapped && !ri_route.afe_started);
  if(scenario==2)assert(!rc && ri_live_stream.completed==8 && ri_live_stream.eos_rendered && strstr(ri_live_stream.note,"native_ios_snapshot"));
  else assert(rc==EBUSY && write_count==4 && !ri_live_stream.complete && !ri_pcm.poisoned);
 }
 for(unsigned scenario=2;scenario<=62;scenario++) {
  if(scenario!=2 && scenario!=61 && scenario!=62)continue;
  stream_reset(2);assert(!ri_stream_test());commands=write_count=0;expected_stream=0x05010000;
  assert(!ri_stream_prepare_live());ri_live_stream.tone=ri_live_stream.queued_pcm=true;expected_tone=expected_queued=true;mode=scenario;
  ri_stream_output_start=host_output_start;ri_stream_output_check=host_output_check;ri_stream_output_stop=host_output_stop;
  int rc=ri_stream_execute(&ri_live_stream,5);assert(output_starts==1 && output_stops==1 && !output_live);
  if(scenario==2)assert(!rc && ri_live_stream.complete && ri_live_stream.eos_rendered && ri_live_stream.completed==16 && !ri_live_stream.mapped && strstr(ri_live_stream.note,"live_ios_queue"));
  if(scenario==61)assert(rc==ETIMEDOUT && ri_pcm.poisoned && ri_live_stream.mapped && ri_live_stream.in_flight);
  if(scenario==62)assert(rc==ENOBUFS && !ri_pcm.poisoned && !ri_live_stream.mapped && !ri_live_stream.complete);
 }
#if RIVER_LONG_RUNTIME
 for(unsigned scenario=63;scenario<=65;scenario++) {
  stream_reset(2);assert(!ri_stream_test());commands=write_count=0;expected_stream=0x05010000;
  assert(!ri_stream_prepare_live());ri_live_stream.tone=ri_live_stream.queued_pcm=true;expected_tone=expected_queued=expected_long=true;
  ri_audio_queue.long_mode=true;mode=scenario;host_gate_now=0;
  ri_stream_output_start=host_output_start;ri_stream_output_check=host_output_check;ri_stream_output_stop=host_output_stop;
  int rc=ri_stream_execute(&ri_live_stream,5);assert(output_stops==1 && !output_live && !ri_audio_queue.waiting_output && !ri_live_stream.mapped);
  if(scenario==63)assert(!rc && output_starts==1 && ri_live_stream.complete && ri_live_stream.completed==244 && ri_live_stream.eos_rendered);
  if(scenario==64)assert(rc==ECANCELED && !output_starts && !ri_live_stream.complete);
  if(scenario==65)assert(rc==ETIMEDOUT && !output_starts && !ri_live_stream.complete && host_gate_now==(RIVER_LISTENING_WINDOW?90000000000ULL:15000000000ULL));
 }
 puts("PASS actual listening gate: no output until GO, long EOS, explicit stop and timeout clean the still-muted route");
#endif
 puts("actual-code zero-PCM map/RUN/WRITE ownership, completion and failure teardown tests passed");
 return 0;
}
