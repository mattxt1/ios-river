#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
typedef uintptr_t vm_offset_t;
static struct {bool active,selected,ngd_read;uint32_t hw_version,ngd_offset,ngd_cfg,ngd_int_en,ngd_int_stat;} ri_slim;
static struct {bool complete;} ri_stream;
static struct {bool muted,afe_started;} ri_route;
static unsigned mode,maps,unmaps,writes,sends,waits;
static bool live,ready,channel_mode,hardware_connected,hardware_defined,hardware_active,remove_pending;
static unsigned channel_sends,fail_index,fail_kind;
static void channel_response(unsigned length);
static uint32_t regs[0x4000/4],tx[8];
static bool ri_slim_live(void){return live;}
static bool ri_route_ready(void){return ready;}
static bool ri_stream_live_prepared(void){return false;}
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
 if(o==0){assert(v==0 || v==1);if(mode==17 && v==1)return;if(mode==16 && !v)return;raw(0,v);if(v){raw(4,0x40e);if(mode!=2){uint8_t cap[]={0xc7,0,0x20,0xd9,0xc5,1,1};receive(cap,7);}}return;}
 if(o==0x18){assert(v && !(v&0x3fffffff) || v==0x1000000 || v==0x10000000 || v==0x11000000);regs[0x1014/4]&=~v;return;}
 assert(o>=0x30 && o<0x50 && !(o&3));unsigned index=(o-0x30)/4;tx[index]=v;
 unsigned length=tx[0]&31;
 if(index!=(length+3)/4-1)return;
 if(channel_mode){channel_response(length);return;}
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
#define printf(...) ((int)0)
#include "channel-actual.h"
#undef printf
static void reset(unsigned m)
{
 memset(&ri_ngd,0,sizeof(ri_ngd));memset(&ri_slim,0,sizeof(ri_slim));memset(regs,0,sizeof(regs));memset(tx,0,sizeof(tx));
 channel_mode=false;mode=m;maps=unmaps=writes=sends=waits=0;live=ready=true;
 ri_slim.active=ri_slim.selected=ri_slim.ngd_read=ri_stream.complete=ri_route.muted=true;
 ri_slim.hw_version=regs[0]=0x1050000;ri_slim.ngd_offset=0x1000;raw(4,0x40c);raw(0xf0,0x10000);
}
int ngd_baseline_main(void)
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
 unsigned modes[]={2,3,4,5,6,7,8,9,10,11,16,17};
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

static void channel_response(unsigned length)
{
 const uint8_t *b=(const uint8_t*)tx;unsigned code=b[1],tid=0,la=ri_ngd.codec_address;
 channel_sends++;assert(channel_sends<=8 && b[2]==0xff);
 switch(code){
 case 0x2d:assert(length==7 && b[3]==la && !b[4] && b[5]==144 && !hardware_connected);tid=b[6];break;
 case 0x21:assert(length==9 && b[3]==(la&31) && b[4]==0x24 && !b[5] && b[6]==0x83 && b[8]==144 && hardware_connected && !hardware_defined);tid=b[7];break;
 case 0x24:assert(length==5 && b[4]==la && hardware_defined);tid=b[3];break;
 case 0x23:assert(length==6 && b[3]==(0x80|(la&31)) && b[5]==144 && hardware_defined);tid=b[4];break;
 case 0x2e:assert(length==6 && b[3]==la && !b[4] && hardware_connected && !hardware_defined);tid=b[5];break;
 default:assert(0);
 }
 assert(tid==0x71+channel_sends);
 bool fail=fail_index==channel_sends;
 bool nack=fail && fail_kind==1;
 if(!nack) {
  if(code==0x2d)hardware_connected=true;
  if(code==0x21)hardware_defined=true;
  if(code==0x23)remove_pending=true;
  if(code==0x24){if(remove_pending){hardware_defined=hardware_active=remove_pending=false;}else hardware_active=true;}
  if(code==0x2e)hardware_connected=false;
 }
 if(!(fail && fail_kind==4))regs[0x1014/4]|=0x80000000;
 if(fail && fail_kind==3)return;
 uint8_t ack[]={0xc5,0x25,0xff,tid,nack?1:0x20};
 if(fail && fail_kind==2)ack[3]++;
 receive(ack,sizeof(ack));
}
static void channel_reset(unsigned index,unsigned kind)
{
 reset(0);assert(!ri_ngd_enumerate());memset(&ri_channel,0,sizeof(ri_channel));
 hardware_connected=hardware_defined=hardware_active=remove_pending=false;
 channel_mode=true;channel_sends=0;fail_index=index;fail_kind=kind;
}
static unsigned callback_count;static int callback_error;
static int active_callback(void) {assert(ri_channel.connected && ri_channel.defined && ri_channel.active);callback_count++;return callback_error;}
static int uncertain_callback(void){ri_channel.uncertain=true;return ETIMEDOUT;}
int main(void)
{
 ngd_baseline_main();
 channel_reset(0,0);assert(!ri_channel_test() && ri_channel.complete);
 assert(ri_channel.commands==6 && ri_channel.acks==6 && !ri_channel.connected && !ri_channel.defined && !ri_channel.active);
 assert(!hardware_connected && !hardware_defined && !hardware_active && !regs[0x1010/4] && regs[0x1000/4]==1);
 assert(ri_channel_test()==EALREADY);
 for(unsigned index=1;index<=6;index++){
  channel_reset(index,1);assert(ri_channel_test()==EREMOTE && !ri_channel.complete && !ri_channel.uncertain);
  if(index<=3)assert(!ri_channel.connected && !ri_channel.defined && !ri_channel.active && !ri_channel.cleanup_error);
  else assert(ri_channel.cleanup_error==EREMOTE);
  assert(ri_channel.failed_opcode || ri_channel.cleanup_error);
 }
 for(unsigned kind=2;kind<=4;kind++)for(unsigned index=1;index<=6;index++){
  channel_reset(index,kind);assert(ri_channel_test()==(kind==2?EIO:ETIMEDOUT));
  assert(ri_channel.uncertain && !ri_channel.complete && channel_sends==index);
  if(kind==2)assert(regs[0x1014/4]&0x40000000);
 }
 channel_reset(0,0);ri_ngd.address_valid=false;assert(ri_channel_test()==EBUSY && !ri_channel.attempted && !channel_sends);
 channel_reset(0,0);ri_ngd.uncertain=true;assert(ri_channel_test()==EBUSY && !channel_sends);
 channel_reset(0,0);raw(0x14,0x40000000);assert(ri_channel_test()==EBUSY && !ri_channel.attempted && !channel_sends);
 channel_reset(0,0);raw(0x10,1);assert(ri_channel_test()==EBUSY && !channel_sends);
 channel_reset(0,0);mode=12;assert(ri_channel_test()==ENOMEM && !ri_channel.attempted);
 channel_reset(0,0);callback_count=0;callback_error=0;ri_channel_active_test=active_callback;assert(!ri_channel_test() && callback_count==1 && ri_channel.complete);
 channel_reset(0,0);callback_count=0;callback_error=EIO;ri_channel_active_test=active_callback;assert(ri_channel_test()==EIO && callback_count==1 && !ri_channel.connected && !ri_channel.defined && !ri_channel.active && !ri_channel.cleanup_error);
 channel_reset(0,0);ri_channel_active_test=uncertain_callback;assert(ri_channel_test()==ETIMEDOUT && ri_channel.uncertain && ri_channel.active && ri_channel.connected && channel_sends==3);
 ri_channel_active_test=NULL;
 puts("actual-code codec sink/connect activate/reconfigure/remove/disconnect and ACK ownership/failure tests passed");
 return 0;
}
