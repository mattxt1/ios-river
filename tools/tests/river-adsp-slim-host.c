#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
typedef uintptr_t vm_offset_t;
struct ri_apr_channel {vm_offset_t info,tx,rx,ipc;uint32_t entry;};
struct ri_adsp_record {uint32_t offset,bytes;};
struct ri_adsp_transport_records {struct ri_adsp_record table[2],info[64],fifo[64];uint32_t end,cached;};
static struct {bool complete;} ri_stream;
static struct {bool muted;} ri_route;
static struct {bool ready,authenticated;} ri_adsp_start;
static uint8_t mem[0x100000],ipc_mem[0x4000],ngd_mem[0x4000];
static vm_offset_t river_smd_smem;
static unsigned scenario,waits,tx_count,flow_acks,unmaps;
static bool route_ready;
static uint32_t ri_rd(vm_offset_t p,unsigned o){uint32_t v;memcpy(&v,(void*)(p+o),4);return v;}
static void raw(vm_offset_t p,unsigned o,uint32_t v){memcpy((void*)(p+o),&v,4);}
static bool ri_adsp_transport_collect(vm_offset_t p,struct ri_adsp_transport_records *r)
{assert(p==river_smd_smem);memset(r,0,sizeof(*r));r->table[0]=(struct ri_adsp_record){0x86300,2048};r->info[2]=(struct ri_adsp_record){0x8ab58,40};r->fifo[2]=(struct ri_adsp_record){0x8ab90,16384};r->end=0x29d10;r->cached=0x2a000;return scenario!=1;}
static bool ri_apr_dsp_ready(vm_offset_t p){return p==river_smd_smem;}
static bool ri_route_ready(void){return route_ready;}
#define VM_WIMG_IO 1
static vm_offset_t ml_io_map_unmappable(unsigned p,unsigned n,unsigned attr){assert((p==0xb010000 || p==0xc140000) && n==0x4000 && attr==1);if(p==0xc140000)return scenario==15?0:(vm_offset_t)ngd_mem;return scenario==2?0:(vm_offset_t)ipc_mem;}
static void ml_io_unmap(vm_offset_t p,unsigned n){assert((p==(vm_offset_t)ipc_mem || p==(vm_offset_t)ngd_mem) && n==0x4000);unmaps++;}
static void ri_apr_write8(vm_offset_t p,uint8_t v){*(uint8_t*)p=v;}
static void ri_apr_ack_flags(const struct ri_apr_channel *c){for(unsigned i=28;i<=30;i++)ri_apr_write8(c->info+i,0);}
static bool ri_apr_indices(const struct ri_apr_channel *c){uint32_t a=ri_rd(c->info,16),b=ri_rd(c->info,12),d=ri_rd(c->info,36),e=ri_rd(c->info,32);return (a|b|d|e)<8192 && !((a|b|d|e)&3);}
static void ri_wait(unsigned n){assert(n==5 || n==10);waits++;}
static void ri_apr_notify(const struct ri_apr_channel *c){assert(c->ipc==(vm_offset_t)ipc_mem+0x1008);}
static void ri_apr_set_state(const struct ri_apr_channel *c,unsigned state){raw(c->info,0,state);if(state==1 && scenario!=3)raw(c->info,20,2);if(!state)raw(c->info,20,4);}
static void queue(const uint32_t *p,unsigned bytes)
{vm_offset_t info=river_smd_smem+0x8ab58,rx=river_smd_smem+0x8ab90+8192;unsigned head=ri_rd(info,36);raw(rx,head,bytes);for(unsigned i=4;i<20;i+=4)raw(rx,(head+i)&8191,0);for(unsigned i=0;i<bytes;i+=4)raw(rx,(head+20+i)&8191,p[i/4]);raw(info,36,(head+20+bytes)&8191);}
static void response_to_tx(const uint32_t *frame);
static void ri_wr(vm_offset_t p,unsigned off,uint32_t value)
{raw(p,off,value);if(p==river_smd_smem+0x8ab58 && off==16){unsigned tail=ri_rd(p,12),bytes=ri_rd(river_smd_smem+0x8ab90,tail);uint32_t f[45]={0};for(unsigned i=0;i<bytes+20;i+=4)f[i/4]=ri_rd(river_smd_smem+0x8ab90,(tail+i)&8191);raw(p,12,value);response_to_tx(f);}}
static void host_barrier(void){}
#define printf(...) ((int)0)
#include "slim-actual.h"
#undef printf
static void response_to_tx(const uint32_t *f)
{
 tx_count++;assert(f[5]==1 && f[7]==1 && !f[9]);
 if(f[6]==2) {
  assert(f[0]==52 && f[8]==0xfffffffe && f[11]==0xffffffff && f[12]==0xfffffffe && f[13]==2 && f[15]==2);
  assert(ri_slim_checksum(f+13)==0xe110);
  uint32_t hello[]={1,2,5,0xfffffffe,0,20,0xffffffff,0xfffffffe,2,0xe110,10,0,0};hello[9]=ri_slim_checksum(hello+8);
  if(scenario==4)hello[0]=3;
  if(scenario==5){hello[10]=8;hello[9]=0xe110;hello[9]=ri_slim_checksum(hello+8);}
  if(scenario==11)hello[6]=7;
  if(scenario==12)hello[3]=1;
  if(scenario==13)hello[7]=1;
  if(scenario==14)hello[3]=hello[7]=0xffff;
  queue(hello,sizeof(hello));
  if(scenario!=6){uint32_t srv[]={1,4,5,0xfffffffe,0,20,0xffffffff,0xfffffffe,4,0x301,1,5,42};queue(srv,sizeof(srv));}
 } else if(f[6]==7) {assert(f[13]==7 && f[14]==1 && f[15]==0x52560001);flow_acks++;}
 else {
  assert(f[6]==1 && f[8]==0x52560001 && f[11]==5 && f[12]==42);
  const uint8_t *q=(const uint8_t *)(f+13);unsigned message=q[3]|q[4]<<8;assert(!q[0] && q[7]==1 && q[8]==4);
  if(message==0x20)assert(f[10]==21 && q[5]==14 && q[10]==0 && q[14]==0x10 && q[15]==4 && q[17]==2);
  else assert(message==0x21 && f[10]==14 && q[5]==7 && q[10]==2);
  if(scenario==10 && message==0x21)return;
  uint32_t p[12]={1,1,5,42,1,14,1,0x52560001};uint8_t *r=(uint8_t*)(p+8);
  r[0]=2;r[1]=q[1];r[2]=q[2];r[3]=q[3];r[4]=q[4];r[5]=7;r[7]=2;r[8]=4;
  if(scenario==7)r[1]++;
  if(scenario==8){r[10]=1;r[12]=0x22;}
  if(scenario==9)r[8]=5;
  queue(p,sizeof(p));
 }
}
static void reset(unsigned s)
{
 memset(ngd_mem,0,sizeof(ngd_mem));raw((vm_offset_t)ngd_mem,0,0x10000);raw((vm_offset_t)ngd_mem,0x1000,1);raw((vm_offset_t)ngd_mem,0x1004,2);
 memset(mem,0,sizeof(mem));memset(&ri_slim,0,sizeof(ri_slim));memset(&ri_slim_channel,0,sizeof(ri_slim_channel));
 scenario=s;waits=tx_count=flow_acks=unmaps=0;route_ready=true;
 river_smd_smem=(vm_offset_t)mem;ri_stream.complete=ri_route.muted=ri_adsp_start.ready=ri_adsp_start.authenticated=true;
 unsigned e=0x86300+64;raw(river_smd_smem,e,0x52435049);raw(river_smd_smem,e+4,0x5254);raw(river_smd_smem,e+20,2);raw(river_smd_smem,e+24,0x201);raw(river_smd_smem,e+28,1);
 raw(river_smd_smem,0x8600c,0x29d10);raw(river_smd_smem,0x86010,0x2a000);raw(river_smd_smem+0x8ab58,20,1);
}
int main(void)
{
 reset(0);assert(ri_slim_discover()==0 && ri_slim.hello && ri_slim.service && ri_slim.peer==5 && ri_slim.port==42);
 assert(ri_slim_discover()==EALREADY);assert(ri_slim_power()==0 && ri_slim.selected && ri_slim.active && !ri_slim.uncertain && flow_acks==2);
 assert(ri_slim_power()==EALREADY && tx_count==5 && !unmaps);
 assert(ri_rd(ri_slim_channel.info,32)==ri_rd(ri_slim_channel.info,36));
 reset(0);ri_stream.complete=false;assert(ri_slim_discover()==EBUSY && !ri_slim.attempted);
 reset(1);assert(ri_slim_discover()==EBUSY && !tx_count);
 reset(2);assert(ri_slim_discover()==ENOMEM && !ri_slim.attempted);
 reset(3);assert(ri_slim_discover()==ETIMEDOUT && ri_slim.close_observed && unmaps==1 && !tx_count);
 reset(4);assert(ri_slim_discover()==EIO && ri_slim.uncertain && ri_slim.close_observed);
 reset(5);assert(ri_slim_discover()==ENOTSUP && ri_slim.uncertain);
 reset(6);assert(ri_slim_discover()==ETIMEDOUT && !ri_slim.service && ri_slim.hello);
 for(unsigned mode=7;mode<=10;mode++){
  reset(mode);assert(ri_slim_discover()==0);int error=ri_slim_power();
  assert(error==(mode==8?EREMOTE:mode==10?ETIMEDOUT:EIO));
  assert(!ri_slim.active && ri_slim.uncertain==(mode!=8));
  if(mode==8)assert(ri_slim.qmi_result==1 && ri_slim.qmi_error==0x22 && !ri_slim.selected);
  if(mode==10)assert(ri_slim.selected);
 }
 for(unsigned m=11;m<=14;m++) {reset(m);assert(ri_slim_discover()==EIO && !ri_slim.hello && ri_slim.close_observed);}
 reset(0);assert(ri_slim_discover()==0);assert(ri_slim.last_destination==0xffffffff && ri_slim.last_control[1]==0x301);
 uint32_t foreign[]={1,4,5,0xfffffffe,0,20,7,0xfffffffe,4,0x301,1,5,42};assert(ri_slim_decode(foreign,sizeof(foreign),0,0)==EIO);
 uint32_t wrong_data[]={1,1,5,42,0,14,0xffffffff,0x52560001,0,0,0,0};assert(ri_slim_decode(wrong_data,sizeof(wrong_data),0x20,1)==EIO);
 reset(0);raw(river_smd_smem+0x8ab58,16,4);assert(ri_slim_discover()==EBUSY && !tx_count);
 reset(0);raw(river_smd_smem,0x86300+64+24,0x101);assert(ri_slim_discover()==EBUSY);
 reset(0);assert(ri_slim_discover()==0);unsigned offset=8180;raw(ri_slim_channel.info,16,offset);raw(ri_slim_channel.info,12,offset);raw(ri_slim_channel.info,36,offset);raw(ri_slim_channel.info,32,offset);assert(ri_slim_power()==0 && ri_slim.active);
 reset(0);assert(ri_slim_discover()==0);ri_slim.service=false;assert(ri_slim_power()==EBUSY && !ri_slim.power_attempted);
 reset(0);assert(ri_slim_ngd_snapshot()==EBUSY && !ri_slim.ngd_attempted);
 reset(0);assert(!ri_slim_discover() && !ri_slim_power());assert(ri_slim_ngd_snapshot()==0 && ri_slim.ngd_read && ri_slim.ngd_offset==0x1000 && ri_slim.ngd_cfg==1 && ri_slim.ngd_status==2);assert(ri_slim_ngd_snapshot()==EALREADY);
 reset(0);assert(!ri_slim_discover() && !ri_slim_power());raw((vm_offset_t)ngd_mem,0,0);assert(!ri_slim_ngd_snapshot() && ri_slim.ngd_offset==0x800);
 reset(0);assert(!ri_slim_discover() && !ri_slim_power());raw((vm_offset_t)ngd_mem,0,0xffffffff);assert(ri_slim_ngd_snapshot()==EIO && !ri_slim.ngd_read);
 reset(15);assert(!ri_slim_discover() && !ri_slim_power());assert(ri_slim_ngd_snapshot()==ENOMEM && !ri_slim.ngd_attempted);
 reset(0);assert(!ri_slim_discover() && !ri_slim_power());ri_slim.uncertain=true;assert(ri_slim_ngd_snapshot()==EBUSY && !ri_slim.ngd_attempted);
 puts("actual-code IPCRTR HELLO/service, QMI select/power, flow ACK, wrap and failure tests passed");
 return 0;
}
