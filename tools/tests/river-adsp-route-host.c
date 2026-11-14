#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
struct ri_apr_request {uint32_t source,destination,token,opcode,response;const uint32_t *payload;unsigned words;};
struct ri_apr_reply {uint32_t status,data[32];unsigned words;bool sent,complete;};
static struct {bool unmapped,asm_closed,poisoned;unsigned transactions;uint32_t rx_header,rx_source,rx_destination,rx_token,rx_opcode;} ri_pcm;
static bool ri_afe_mute_hold,ri_speaker_ok;
static unsigned ri_speaker_lock,lock_depth,commands,mode;
static uint32_t mute_control,fail_opcode,fail_status,cleanup_fail_opcode,ops[32],digital;
static bool timeout_failure,mute_failure;
static void lck_mtx_lock(unsigned *p) {assert(p==&ri_speaker_lock && !lock_depth);lock_depth++;}
static void lck_mtx_unlock(unsigned *p) {assert(p==&ri_speaker_lock && lock_depth==1);lock_depth--;}
static bool ri_cs35l41_rd(uint32_t reg,uint32_t *out) {assert(reg==0x2024 || reg==0x6000);*out=reg==0x2024?mute_control:digital;return !mute_failure;}
static bool ri_cs35l41_wr(uint32_t reg,uint32_t value) {assert(reg==0x2024 || reg==0x6000);if(reg==0x2024)mute_control=value;else digital=value;return !mute_failure;}
static int ri_pcm_transaction(struct ri_apr_request *r,struct ri_apr_reply *p)
{
 memset(p,0,sizeof(*p));assert(!lock_depth && commands<32 && !ri_pcm.poisoned);
 ops[commands++]=r->opcode;ri_pcm.transactions++;p->sent=true;
 if(r->opcode==fail_opcode || r->opcode==cleanup_fail_opcode) {
  p->status=fail_status;
  if(timeout_failure){ri_pcm.poisoned=true;return ETIMEDOUT;}
  p->complete=true;return EREMOTE;
 }
 p->complete=true;
 switch(r->opcode) {
 case 0x100ef: {
  uint32_t expected[]={(36u<<16)|0x4000,0,0,0,0x1020c,0x10212,24,1,16u<<16,1u<<16,144,0,48000};
  assert(r->source==0x504 && r->destination==0x404 && r->words==13 && !r->response);
  assert(!memcmp(r->payload,expected,sizeof(expected)));break;
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
  assert(ri_afe_mute_hold && (mute_control&0x10));break;
 case 0x10db3: {
  uint32_t session=(r->source>>24)&0xff;
  assert(session==2 || session==3);assert(r->source==(((session<<8)|1)<<16|0x507));
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
  assert(r->payload[0]==0 && r->payload[1]==1 && (r->payload[2]==((1u<<16)|2) || r->payload[2]==((1u<<16)|3)) && r->payload[3]==0);break;
 case 0x10327:assert(r->source==0x40000508 && r->destination==0x408 && !r->words);break;
 default:assert(0);
 }
 return 0;
}
static int quiet_printf(const char *format,...) {(void)format;return 0;}
#define printf quiet_printf
#include "route-actual.h"
#undef printf
static void reset(unsigned scenario)
{
 memset(&ri_pcm,0,sizeof(ri_pcm));memset(&ri_route,0,sizeof(ri_route));
 ri_pcm.unmapped=ri_pcm.asm_closed=ri_speaker_ok=true;
 ri_afe_mute_hold=false;commands=lock_depth=0;mute_control=0xa5000001;digital=0;
 mode=scenario;fail_opcode=cleanup_fail_opcode=0;fail_status=7;timeout_failure=mute_failure=false;
}
int main(void)
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
