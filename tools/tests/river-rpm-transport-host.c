#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uintptr_t vm_offset_t;
static uint32_t mem[2048], ipc_words[4], smem[4096];
static unsigned writes, ticks, request_count, mode, barriers, locked, submitted_start;
static unsigned ri_rpm_lock;
#define LCK_MTX_ASSERT_OWNED 1
static void lck_mtx_assert(unsigned *lock, unsigned owned)
{ assert(lock==&ri_rpm_lock && owned==1 && locked); }
static uint32_t ri_rd(vm_offset_t base,unsigned offset)
{
 assert(!(offset&3));
 uintptr_t address=base+offset;
 assert((address>=(uintptr_t)mem && address<(uintptr_t)(mem+2048)) ||
        (address>=(uintptr_t)smem && address<(uintptr_t)(smem+4096)));
 return *(uint32_t *)address;
}
static void reply(uint32_t id, bool nack)
{
 unsigned tail=mem[84/4];
 uint32_t words[]={nack?32:20,0,0,0,0,0x716572,nack?24:12,0x2367736d,4,id,0x727265,4,0x626164};
 unsigned size=nack?52:40;
 for(unsigned i=0;i<size;i+=4) mem[(0xc00+((tail+i)&1023))/4]=words[i/4];
 mem[84/4]=(tail+size)&1023;
}
static void ri_wr(vm_offset_t base,unsigned offset,uint32_t value)
{
 uintptr_t address=base+offset;
 assert(!(address&3));
 if(base==(uintptr_t)ipc_words) {
  assert(offset==0 && value==1);
  if(mem[40/4]!=mem[36/4]) {
   assert(barriers>=2);
   unsigned tail=submitted_start;
   uint32_t id=mem[(0x800+((tail+28)&1023))/4];
   request_count++;
   mem[36/4]=mem[40/4];
   if(mode==0) reply(id,false);
   if(mode==1) { reply(id-1,false); reply(id,false); }
   if(mode==2) reply(id,true);
   if(mode==4) reply(id-1,false);
  }
 } else {
  assert(address>=(uintptr_t)mem && address<(uintptr_t)(mem+2048));
  unsigned relative=(unsigned)(address-(uintptr_t)mem);
  assert((relative>=0x800 && relative<0xc00) || relative==40 || relative==24 ||
      relative==20 || relative==80 || relative==68);
  if(relative==40) { assert(barriers>=1); submitted_start=mem[40/4]; }
 }
 writes++; *(uint32_t *)address=value;
}
static void host_barrier(void) { barriers++; }
static void delay(unsigned us)
{
 assert(us==1000); ticks++;
 if(mode==5 && ticks==3) mem[36/4]=mem[40/4]; /* remote drains old TX */
 if(mode==6 && request_count && ticks==3) {
  unsigned head=mem[40/4];
  reply(mem[(0x800+((head-84+28)&1023))/4],false);
 }
 if(mode==7 && ticks==2) mem[44/4]=0;
}
#include "rpm-reply-actual.h"
#include "rpm-transport-actual.h"
static void reset(unsigned variant,unsigned head)
{
 memset(mem,0,sizeof(mem)); memset(ipc_words,0,sizeof(ipc_words));
 mem[0]=mem[44/4]=2; mem[40/4]=mem[36/4]=head;
 writes=ticks=request_count=barriers=0; mode=variant; locked=1;
 ri_rpm_channel.next_id=0x52490001; ri_rpm_channel.ready=false; ri_rpm_channel.boot_votes=0;
}
static struct ri_rpm_kv kv[]={{0x6e657773,1},{0x7675,1300000},{0x616d,10}};
static int send(bool boot,unsigned count,uint32_t *id)
{ return ri_rpm_request_locked((uintptr_t)mem,(uintptr_t)ipc_words,boot,0,0x616f646c,19,kv,count,id); }
static void no_send(int expected,bool boot,unsigned count)
{
 uint32_t before[2048], id=99; memcpy(before,mem,sizeof(mem));
 assert(send(boot,count,&id)==expected && !id && !writes && !request_count);
 assert(!memcmp(before,mem,sizeof(mem)));
}
int main(void)
{
 uint32_t id=0;
 reset(0,0); assert(send(true,3,&id)==RI_RPM_REPLY_ACK && id==0x52490001);
 uint32_t expected[]={64,0,0,0,0,0x716572,56,0x52490001,0,0x616f646c,19,36,
     0x6e657773,4,1,0x7675,4,1300000,0x616d,4,10};
 assert(!memcmp(mem+0x800/4,expected,sizeof(expected)) && mem[40/4]==84);
 assert(send(true,3,&id)==RI_RPM_REPLY_ACK && id==0x52490002 && request_count==2);
 reset(1,1000); assert(send(true,3,&id)==RI_RPM_REPLY_ACK && ticks==1);
 for(unsigned i=0;i<21;i++) assert(mem[(0x800+((1000+i*4)&1023))/4]==expected[i]);
 reset(2,0); assert(send(true,3,&id)==RI_RPM_REPLY_NACK);
 reset(3,0); assert(send(true,3,&id)==RI_RPM_REQUEST_TIMEOUT && ticks==500 && request_count==1);
 reset(4,0); assert(send(true,3,&id)==RI_RPM_REQUEST_TIMEOUT && ticks==500);
 reset(6,0); assert(send(true,3,&id)==RI_RPM_REPLY_ACK && ticks==3);
 reset(7,0); assert(send(true,3,&id)==RI_RPM_REPLY_BAD && ticks==2);
 reset(0,0); mem[36/4]=4; no_send(RI_RPM_REQUEST_TIMEOUT,true,3); assert(ticks==500);
 reset(0,0); mem[36/4]=84; no_send(RI_RPM_REQUEST_TIMEOUT,true,3); /* reserve 4 bytes */
 reset(0,0); mem[36/4]=88; assert(send(true,3,&id)==RI_RPM_REPLY_ACK); /* exact capacity */
 reset(5,0); mem[36/4]=4;
 assert(send(true,3,&id)==RI_RPM_REQUEST_TIMEOUT && ticks==503 && request_count==1);
 for(unsigned i=0;i<4;i++) {
  reset(0,0); mem[(i<2?40:36)/4]=(i&1)?1024:1; no_send(RI_RPM_REPLY_BAD,true,3);
 }
 for(unsigned i=0;i<2;i++) {
  reset(0,0); mem[i?44/4:0]=1; no_send(RI_RPM_REPLY_BAD,true,3);
 }
 reset(0,0); no_send(RI_RPM_REQUEST_GATED,false,3);
 ri_rpm_channel.ready=true; assert(send(false,3,&id)==RI_RPM_REPLY_ACK);
 reset(0,0); no_send(RI_RPM_REPLY_BAD,true,0); no_send(RI_RPM_REPLY_BAD,true,4);
 ri_rpm_channel.next_id=0xffffffff; no_send(RI_RPM_REPLY_BAD,true,3);
 ri_rpm_channel.next_id=0; no_send(RI_RPM_REPLY_BAD,true,3);
 reset(0,0); assert(send(true,1,&id)==RI_RPM_REPLY_ACK && mem[40/4]==60);
 reset(0,0); assert(send(true,2,&id)==RI_RPM_REPLY_ACK && mem[40/4]==72);
 memset(smem,0,sizeof(smem)); assert(!ri_rpm_layout((uintptr_t)smem));
 unsigned info=0xd0+18*16, fifo=0xd0+342*16;
 smem[info/4]=smem[fifo/4]=1; smem[(info+8)/4]=88;
 smem[(info+12)/4]=smem[(fifo+12)/4]=0x60000;
 smem[(fifo+4)/4]=smem[(fifo+8)/4]=0x800;
 assert(ri_rpm_layout((uintptr_t)smem));
 for(unsigned i=0;i<8;i++) {
  unsigned index=((i<4?info:fifo)/4)+(i%4); uint32_t saved=smem[index];
  smem[index]^=1; assert(!ri_rpm_layout((uintptr_t)smem)); smem[index]=saved;
 }
 puts("actual RPM transport: packets, wrap, capacity, delayed drain/ACK, stale/NACK/timeout, IDs, ownership assertion and readiness gates PASS");
}
