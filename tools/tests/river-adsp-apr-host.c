#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
typedef uintptr_t vm_offset_t;
#define VM_WIMG_IO 0
#define LCK_MTX_ASSERT_OWNED 1
static uint32_t memory[0x100000/4],ipc_words[0x4000/4];
static vm_offset_t river_smd_smem;
static unsigned ri_adsp_lock;
static struct {bool ready,authenticated;} ri_adsp_start;
static unsigned inf,tx,rx,entry,slave;
static unsigned mode,writes,byte_writes,maps,unmaps,wait_ms,doorbells,packet_words,barriers;
static unsigned packet_barrier,head_barrier,rx_reads,mutate_rx_head;
static bool map_fail,map_race;
static uint32_t ri_rd(vm_offset_t base,unsigned offset)
{
 uintptr_t address=base+offset,start=(uintptr_t)memory;
 assert(!(address&3) && address>=start && address-start<sizeof(memory));
 unsigned pos=(unsigned)(address-start);uint32_t value=memory[pos/4];
 if(pos>=rx && pos<rx+8192)rx_reads++;
 if(pos==inf+36 && mutate_rx_head) {mutate_rx_head=0;memory[pos/4]=8192;}
 return value;
}
static void ri_wr(vm_offset_t,unsigned,uint32_t);
static void host_write8(vm_offset_t,uint8_t);
static void ri_wait(unsigned);
static void host_barrier(void) {barriers++;}
static void lck_mtx_assert(unsigned *lock,unsigned owned) {assert(lock==&ri_adsp_lock && owned==1);}
static vm_offset_t ml_io_map_unmappable(unsigned pa,unsigned size,unsigned attr)
{
 assert(pa==0x0b010000 && size==0x4000 && attr==0);maps++;
 if(map_race)memory[inf/4]=2;
 return map_fail?0:(uintptr_t)ipc_words;
}
static void ml_io_unmap(vm_offset_t base,unsigned bytes)
{assert(base==(uintptr_t)ipc_words && bytes==0x4000);unmaps++;}
static int quiet_printf(const char *,...) __attribute__((format(printf,1,2)));
static int quiet_printf(const char *format,...) {(void)format;return 0;}
#include "transport-collect-actual.h"
#include "remote-snapshot-actual.h"
#define printf quiet_printf
#include "apr-actual.h"
#undef printf
static unsigned alloc(unsigned id,unsigned bytes)
{
 unsigned h=0x86000+memory[0x8600c/4],padded=(bytes+7)&~7u;
 assert(h+16+padded<=0xaa000);
 memory[h/4]=(id<<16)|0xa5a5;memory[(h+4)/4]=padded;
 memory[(h+8)/4]=padded-bytes;memory[0x8600c/4]+=16+padded;
 return h+16;
}
static void name_at(unsigned pos,const char *name)
{assert(strlen(name)<20);memcpy((char *)memory+pos,name,strlen(name)+1);}
static void reset(unsigned scenario)
{
 memset(memory,0,sizeof(memory));memset(ipc_words,0,sizeof(ipc_words));memset(&ri_apr,0,sizeof(ri_apr));
 strcpy(ri_apr_note,"not requested");river_smd_smem=(uintptr_t)memory;
 ri_adsp_start.ready=ri_adsp_start.authenticated=true;
 mode=scenario;writes=byte_writes=maps=unmaps=wait_ms=doorbells=packet_words=barriers=0;
 packet_barrier=head_barrier=rx_reads=mutate_rx_head=0;map_fail=map_race=false;
 memory[0xff000/4]=0x434f5424;memory[0xff004/4]=1;memory[0xff008/4]=7;
 memory[0xff080/4]=0x86000;memory[0xff084/4]=0x24000;memory[0xff08c/4]=0x20000;memory[0xff090/4]=32;
 memory[0x86000/4]=0x54525024;memory[0x86004/4]=0x20000;memory[0x86008/4]=0x24000;
 memory[0x8600c/4]=32;memory[0x86010/4]=0x24000;
 unsigned out=alloc(429,340),in=alloc(443,340);
 assert(out==0x86030 && in==0x86198);
 memory[out/4]=memory[in/4]=0x504d5324;memory[(out+4)/4]=memory[(in+4)/4]=1;
 memory[(out+8)/4]=0x20000;memory[(in+8)/4]=2;
 memory[(out+12)/4]=memory[(in+12)/4]=0x10010;
 name_at(out+20,"master-kernel");name_at(in+20,"slave-kernel");slave=in+36;memory[slave/4]=6;
 unsigned table=alloc(13,2048);entry=table+6*32;name_at(entry,"apr_audio_svc");
 memory[(entry+20)/4]=6;memory[(entry+24)/4]=0x201;memory[(entry+28)/4]=1;
 inf=alloc(20,40);tx=alloc(344,16384);rx=tx+8192;memory[(inf+20)/4]=1;
}
static unsigned append(unsigned tail,const uint32_t *body,unsigned bytes)
{
 memory[(rx+tail)/4]=bytes;
 for(unsigned i=4;i<20;i+=4)memory[(rx+((tail+i)&8191))/4]=0;
 for(unsigned i=0;i<bytes;i+=4)memory[(rx+((tail+20+i)&8191))/4]=body[i/4];
 unsigned end=(tail+20+bytes)&8191;memory[(inf+36)/4]=end;return end;
}
static void respond(void)
{
 uint32_t token=ri_apr.token;
 uint32_t response[7]={(24u<<16)|0x150,0x403,0x503,token,0x1290d,1,0};
 if(mode==2 || mode==6)return; /* timeout / delayed response */
 unsigned h=memory[(inf+36)/4];
 if(mode==1) {
  response[3]=token-1;h=append(h,response,24);response[3]=token;
  uint32_t ack[]={(28u<<16)|0x150,0x403,0x503,token,0x110e8,0x1290c,0};h=append(h,ack,28);
 }
 if(mode==3) {
  response[0]=(28u<<16)|0x150;response[4]=0x110e8;response[5]=0x1290c;response[6]=7;
  append(h,response,28);return;
 }
 if(mode==5)response[5]=0; /* valid transport, DSP says not ready */
 if(mode==7 || mode==13)response[0]^=1; /* malformed APR version */
 if(mode==8)response[3]=token-1; /* only a stale token, never success */
 if(mode==9) {memory[(rx+h)/4]=512;memory[(inf+36)/4]=(h+24)&8191;return;}
 append(h,response,24);
}
static void host_write8(vm_offset_t address,uint8_t value)
{
 uintptr_t base=(uintptr_t)memory;assert(address>=base && address-base<sizeof(memory));
 unsigned offset=(unsigned)(address-base)-inf;
 assert(offset==4 || offset==5 || offset==6 || offset==8 || offset==9 || offset==10 || offset==11 ||
     offset==28 || offset==29 || offset==30);
 byte_writes++;*((uint8_t *)address)=value;
}
static void ri_wr(vm_offset_t base,unsigned offset,uint32_t value)
{
 uintptr_t address=base+offset;
 if(base==(uintptr_t)ipc_words+0x1008) {
  assert(offset==0 && value==0x100);doorbells++;
  unsigned state=memory[inf/4];
  if(state==1 && mode!=4) {memory[(inf+20)/4]=2;((uint8_t *)memory)[inf+10]=0;}
  if(state==0 && mode!=13) {memory[(inf+20)/4]=0;((uint8_t *)memory)[inf+10]=0;}
  if(state==2 && memory[(inf+16)/4]!=memory[(inf+12)/4]) {
   assert(packet_words==10 && barriers>head_barrier && ((uint8_t *)memory)[inf+8]==1);
   uint32_t expected[]={20,0,0,0,0,(20u<<16)|0x250,0x503,0x403,0x52410001,0x1290c};
   assert(!memcmp((char *)memory+tx,expected,sizeof(expected)));
   memory[(inf+12)/4]=memory[(inf+16)/4];respond();
  }
  *(uint32_t *)address=value;writes++;return;
 }
 uintptr_t start=(uintptr_t)memory;assert(!(address&3) && address>=start && address-start<sizeof(memory));
 unsigned pos=(unsigned)(address-start);
 assert(pos==inf || pos==inf+16 || pos==inf+32 || (pos>=tx && pos<tx+8192));
 if(pos>=tx && pos<tx+8192) {packet_words++;packet_barrier=barriers;}
 if(pos==inf+16 && value) {assert(packet_words==10 && barriers>packet_barrier);head_barrier=barriers;}
 memory[pos/4]=value;writes++;
}
static void ri_wait(unsigned ms)
{
 assert(ms==5 || ms==10 || ms==200);wait_ms+=ms;
 if(ms==200 && mode==10)memory[(inf+12)/4]=4; /* no TX capacity */
 if(ms==200 && mode==12)memory[slave/4]=1; /* fatal after open */
 if(mode==6 && ri_apr.sent && ri_apr.rx_waits==2) {
  uint32_t response[]={(24u<<16)|0x150,0x403,0x503,ri_apr.token,0x1290d,1};
  append(memory[(inf+36)/4],response,24);
 }
}
static int query(void)
{
 int error=ri_apr_query_impl();assert(unmaps==maps-(map_fail?1:0));return error;
}
static void no_write(int expected)
{assert(query()==expected && !writes && !byte_writes && !ri_apr.attempted);}
int main(void)
{
 reset(0);assert(query()==0 && ri_apr.opened && ri_apr.received && ri_apr.state==1);
 assert(ri_apr.sent && ri_apr.tx_bytes==40 && ri_apr.rx_bytes==44 && !ri_apr.close_requested);
 unsigned prior=writes;assert(ri_apr_query_impl()==EALREADY && writes==prior);
 assert(strstr(ri_apr_note,"phase=core_state") && strstr(ri_apr_note,"pcm_transport=0"));
 reset(1);assert(query()==0 && ri_apr.stale==1 && ri_apr.acks==1 && ri_apr.rx_bytes==136);
 reset(2);assert(query()==ETIMEDOUT && ri_apr.close_observed && !ri_apr.opened && wait_ms==1200);
 reset(3);assert(query()==EIO && ri_apr.reply_error==7 && ri_apr.close_observed);
 reset(4);assert(query()==ETIMEDOUT && ri_apr.open_waits==500 && !ri_apr.sent && ri_apr.close_observed);
 reset(5);assert(query()==EAGAIN && ri_apr.received && !ri_apr.state && ri_apr.close_requested);
 reset(6);assert(query()==0 && ri_apr.rx_waits==3);
 reset(7);assert(query()==EIO && !ri_apr.rx_bytes && ri_apr.close_observed);
 reset(8);assert(query()==ETIMEDOUT && ri_apr.stale==1 && !ri_apr.received);
 reset(9);assert(query()==ETIMEDOUT && !ri_apr.rx_bytes && ri_apr.close_observed);
 reset(10);assert(query()==ETIMEDOUT && ri_apr.tx_waits==100 && !packet_words && !ri_apr.sent);
 reset(0);memory[(entry+16)/4]=0x12345678;memory[(entry+12)/4]|=0x55aa0000;
 assert(query()==0); /* Bytes after the NUL are not part of the name. */
 reset(13);assert(query()==EIO && ri_apr.close_requested && !ri_apr.close_observed && !ri_apr.opened);
 reset(12);assert(query()==EBUSY && !ri_apr.sent && ri_apr.close_observed);
 reset(0);ri_adsp_start.ready=false;no_write(EBUSY);
 reset(0);ri_adsp_start.authenticated=false;no_write(EBUSY);
 reset(0);memory[slave/4]=1;no_write(EBUSY);
 reset(0);river_smd_smem=0;no_write(EBUSY);
 reset(0);memory[(entry+20)/4]=7;no_write(EBUSY);
 reset(0);memory[(entry+24)/4]=1;no_write(EBUSY);
 reset(0);memory[(entry+28)/4]=2;no_write(EBUSY);
 reset(0);memory[inf/4]=2;no_write(EBUSY);
 reset(0);memory[(inf+20)/4]=2;no_write(EBUSY);
 for(unsigned k=0;k<4;k++) {
  reset(0);memory[(inf+(unsigned[]){12,16,32,36}[k])/4]=4;no_write(EBUSY);
 }
 reset(0);map_fail=true;no_write(ENOMEM);
 reset(0);map_race=true;no_write(EBUSY);
 /* Exercise actual RX wrap, partial frames and immutable malformed framing. */
 reset(0);memory[inf/4]=memory[(inf+20)/4]=2;
 struct ri_apr_channel ch;assert(ri_apr_resolve((uintptr_t)memory,&ch));ch.ipc=(uintptr_t)ipc_words+0x1008;
 uint32_t response[]={(24u<<16)|0x150,0x403,0x503,42,0x1290d,7};
 memory[(inf+32)/4]=8180;append(8180,response,24);uint32_t value=0;
 assert(ri_apr_receive(&ch,42,&value)==RI_APR_STATE && value==7 && memory[(inf+32)/4]==32);
 reset(0);memory[inf/4]=memory[(inf+20)/4]=2;assert(ri_apr_resolve((uintptr_t)memory,&ch));
 ch.ipc=(uintptr_t)ipc_words+0x1008;append(0,response,24);memory[(inf+36)/4]=40;
 assert(ri_apr_receive(&ch,42,&value)==RI_APR_WAIT && !writes && !byte_writes);
 memory[(inf+36)/4]=44;memory[rx/4]=0xffffffff;
 assert(ri_apr_receive(&ch,42,&value)==RI_APR_BAD && !writes && !byte_writes);
 reset(0);memory[inf/4]=memory[(inf+20)/4]=2;assert(ri_apr_resolve((uintptr_t)memory,&ch));
 ch.ipc=(uintptr_t)ipc_words+0x1008;append(0,response,24);mutate_rx_head=1;
 assert(ri_apr_receive(&ch,42,&value)==RI_APR_BAD && !rx_reads && !writes && !byte_writes);
 for(unsigned i=0;i<6;i++) {
  uint32_t bad[6];memcpy(bad,response,sizeof(bad));bad[i]^=1;
  int result=ri_apr_decode(bad,24,42,&value);
  if(i==0)assert(result==RI_APR_BAD);
  else if(i<5)assert(result==RI_APR_STALE);
  else assert(result==RI_APR_STATE); /* Any returned state is data, not framing. */
 }
 assert(ri_apr_decode(response,24,0,&value)==RI_APR_BAD);
 puts("actual SMD/APR: owned discovery, byte flags/handshake, exact CORE frame/token, stale/basic/NACK, bounds/partial/wrap/races, timeouts and close protocol PASS");
}
