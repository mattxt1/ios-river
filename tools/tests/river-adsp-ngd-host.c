#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
typedef uintptr_t vm_offset_t;
static struct {bool active,selected,ngd_read;uint32_t hw_version,ngd_offset,ngd_cfg,ngd_int_en,ngd_int_stat;} ri_slim;
static struct {bool complete;} ri_stream;
static struct {bool muted;} ri_route;
static unsigned mode,maps,unmaps,writes,sends,waits;
static bool live,ready;
static uint32_t regs[0x4000/4],tx[8];
static bool ri_slim_live(void){return live;}
static bool ri_route_ready(void){return ready;}
#define VM_WIMG_IO 1
static vm_offset_t ml_io_map_unmappable(unsigned p,unsigned n,unsigned a){assert(p==0xc140000 && n==0x4000 && a==1);maps++;return mode==12?0:(vm_offset_t)regs;}
static void ml_io_unmap(vm_offset_t p,unsigned n){assert(p==(vm_offset_t)regs && n==0x4000);unmaps++;}
static uint32_t ri_rd(vm_offset_t p,unsigned o){assert(p+o>=(vm_offset_t)regs && p+o<(vm_offset_t)regs+sizeof(regs));return *(uint32_t*)(p+o);}
static void raw(unsigned off,uint32_t v){regs[(0x1000+off)/4]=v;}
static void receive(const uint8_t *b,unsigned n){uint32_t tmp[8]={0};memcpy(tmp,b,n);for(unsigned i=0;i<(n+3)/4;i++)raw(0x70+i*4,tmp[i]);regs[0x1014/4]|=0x40000000;}
static void ri_wr(vm_offset_t p,unsigned o,uint32_t v)
{
 assert(p==(vm_offset_t)regs+0x1000);writes++;
 assert(o!=0x10 && o!=8); /* no interrupt enable or message-DMA setup */
 if(o==0){assert(v==0 || v==1);if(mode==16 && !v)return;raw(0,v);if(v){raw(4,0x40e);if(mode!=2){uint8_t cap[]={0xc7,0,0x20,0xd9,0xc5,1,1};receive(cap,7);}}return;}
 if(o==0x18){assert(v && !(v&0x3fffffff) || v==0x1000000 || v==0x10000000 || v==0x11000000);regs[0x1014/4]&=~v;return;}
 assert(o>=0x30 && o<0x50 && !(o&3));unsigned index=(o-0x30)/4;tx[index]=v;
 unsigned length=tx[0]&31;
 if(index!=(length+3)/4-1)return;
 sends++;assert(sends<=2);
 if(sends==1){assert(length==7 && tx[0]==0xd9ff01c7 && tx[1]==0x101c5);if(mode==3 || mode==16)return;}
 else {assert(length==10 && tx[0]==0x71ff0d4a && tx[1]==0x63600000 && tx[2]==0x1fa);}
 regs[0x1014/4]|=mode==4?0x02000000:0x80000000;
 if(sends==2 && mode!=9){
  uint8_t reply[]={0xcb,0xe,0x20,0x71,0,0,0x60,0x63,0xfa,1,0x42};
  if(mode==5)reply[4]++;
  if(mode==6)reply[3]++;
  if(mode==7)memset(reply+4,0,6);
  if(mode==8){reply[0]=0xc5;reply[1]=0x25;reply[4]=0;receive(reply,5);return;}
  if(mode==10)reply[0]=0xc3;
  if(mode==11)reply[0]=0x4b;
  receive(reply,11);
 }
}
static void ri_wait(unsigned n){assert(n==5);waits++;}
static void host_barrier(void){}
#define printf(...) ((int)0)
#include "ngd-actual.h"
#undef printf
static void reset(unsigned m)
{
 memset(&ri_ngd,0,sizeof(ri_ngd));memset(&ri_slim,0,sizeof(ri_slim));memset(regs,0,sizeof(regs));memset(tx,0,sizeof(tx));
 mode=m;maps=unmaps=writes=sends=waits=0;live=ready=true;
 ri_slim.active=ri_slim.selected=ri_slim.ngd_read=ri_stream.complete=ri_route.muted=true;
 ri_slim.hw_version=regs[0]=0x1050000;ri_slim.ngd_offset=0x1000;raw(4,0x40c);raw(0xf0,0x10000);
}
int main(void)
{
 reset(0);assert(!ri_ngd_enumerate());assert(ri_ngd.master && ri_ngd.reported && ri_ngd.address_valid && ri_ngd.codec_address==0x42 && ri_ngd.enabled && !ri_ngd.uncertain);
 assert(maps==unmaps && sends==2 && ri_ngd.tx_count==2 && ri_ngd.rx_count==2 && !regs[0x1010/4] && regs[0x1000/4]==1);
 assert(ri_ngd_enumerate()==EALREADY);
 reset(0);ri_slim.active=false;assert(ri_ngd_enumerate()==EBUSY && !maps && !writes);
 reset(0);ri_slim.ngd_read=false;assert(ri_ngd_enumerate()==EBUSY && !maps);
 reset(0);ri_slim.hw_version++;assert(ri_ngd_enumerate()==EBUSY && !maps);
 reset(0);raw(0,1);assert(ri_ngd_enumerate()==EBUSY && !writes && maps==unmaps);
 reset(0);raw(0x10,1);assert(ri_ngd_enumerate()==EBUSY && !writes);
 reset(0);raw(0x14,0x40000000);assert(ri_ngd_enumerate()==EBUSY && !writes);
 reset(12);assert(ri_ngd_enumerate()==ENOMEM && !writes && !ri_ngd.attempted);
 unsigned modes[]={2,3,4,5,6,7,8,9,10,11,16};
 for(unsigned i=0;i<sizeof(modes)/sizeof(modes[0]);i++){
  reset(modes[i]);int error=ri_ngd_enumerate();
  assert(error==(mode==2 || mode==3 || mode==9 || mode==16?ETIMEDOUT:mode==7?ENXIO:mode==8?EREMOTE:mode==11?ENOTSUP:EIO));
  assert(maps==unmaps && !ri_ngd.address_valid && !regs[0x1010/4]);
  if(mode==16)assert(ri_ngd.restore_error==EIO && ri_ngd.enabled && !ri_ngd.disabled);
  else assert(!ri_ngd.enabled && ri_ngd.disabled && !ri_ngd.restore_error && !regs[0x1000/4]);
  if(mode==5 || mode==6 || mode==10 || mode==11)assert(regs[0x1014/4]&0x40000000);
 }
 puts("actual-code polled NGD capability, satellite report, codec address and ownership/failure tests passed");
 return 0;
}
