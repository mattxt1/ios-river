#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
typedef uintptr_t vm_offset_t;
static struct {bool attempted;} ri_audio_normal;
static struct {unsigned tid,commands,acks,last_opcode,last_tid,last_ack,failed_opcode,failed_tid,failed_status,cleanup_error;bool uncertain;} ri_channel;
static struct {unsigned tid;bool uncertain;} ri_mic;
static struct {unsigned ack_tid,ack_status,tx_count;bool acked,ack_waiting;} ri_ngd;
static unsigned expected,commands,waits;static bool suppress_ack;
static int ri_ngd_send(vm_offset_t base,const uint32_t *words,unsigned total){assert(base==0x1000 && total==7 && ((const uint8_t*)words)[6]==expected && ri_ngd.ack_tid==expected && ri_ngd.ack_waiting);ri_ngd.tx_count++;commands++;return 0;}
static int ri_ngd_poll(vm_offset_t base,bool *sent){assert(base==0x1000 && !*sent);if(!suppress_ack){ri_ngd.acked=true;ri_ngd.ack_status=0x20;}return 0;}
static void ri_wait(unsigned ms){assert(ms==5);waits++;}
#include "channel-actual.h"
#include "capture-actual.h"
int main(void){
 ri_audio_normal.attempted=true;
 for(unsigned i=1;i<=600;i++){
  uint8_t payload[4]={0};expected=1+(i-1)%239;
  assert(!ri_channel_command(0x1000,0x2d,payload,4,3,false));assert(ri_channel.tid==i && ri_channel.last_tid==expected && ri_channel.commands==i && ri_channel.acks==i);
  assert(!ri_mic_channel_command(0x1000,0x2c,payload,4,3));assert(ri_mic.tid==i && !ri_mic.uncertain && !ri_channel.uncertain);
 }
 assert(commands==1200 && !waits && !ri_ngd.ack_waiting);
 ri_audio_normal.attempted=false;ri_channel.tid=ri_mic.tid=239;uint8_t payload[4]={0};assert(ri_channel_command(0x1000,0x2d,payload,4,3,false)==EINVAL && ri_mic_channel_command(0x1000,0x2c,payload,4,3)==EINVAL && commands==1200);
 ri_audio_normal.attempted=true;ri_channel.tid=ri_mic.tid=~0u;assert(ri_channel_command(0x1000,0x2d,payload,4,3,false)==EINVAL && ri_mic_channel_command(0x1000,0x2c,payload,4,3)==EINVAL && commands==1200);
 ri_channel.tid=0;expected=1;suppress_ack=true;assert(ri_channel_command(0x1000,0x2d,payload,4,3,false)==ETIMEDOUT && ri_channel.uncertain && !ri_ngd.ack_waiting);
 ri_mic.tid=0;assert(ri_mic_channel_command(0x1000,0x2c,payload,4,3)==ETIMEDOUT && ri_mic.uncertain && !ri_ngd.ack_waiting);
 puts("PASS actual channel identities: 600 commands per direction, monotonic logical sequences, bounded 8-bit wire IDs, exact ACK ownership, diagnostic cap preserved, logical overflow and missing ACK rejected");
}
