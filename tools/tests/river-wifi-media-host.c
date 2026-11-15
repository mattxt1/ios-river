#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
typedef uint64_t user_addr_t;
struct river_ifmediareq {char name[16];int ifm_current,ifm_mask,ifm_status,ifm_active,ifm_count;};
static unsigned calls;static int word;static user_addr_t dest;static int fault;
static int copyout(const void *p,user_addr_t address,size_t length){assert(length==4);memcpy(&word,p,4);dest=address;calls++;return fault?EFAULT:0;}
#include "actual.h"
int main(void){
 for(unsigned size=40;size<=48;size+=4)for(int live=0;live<=1;live++) {
  union {uint64_t align;uint8_t b[64];} data;memset(&data,0,sizeof(data));
  struct river_ifmediareq *m=(void *)data.b;unsigned long cmd=0xc0006938u|(size<<16);
  assert(!river_wifi_media_query(cmd,m,live));assert(m->ifm_count==1 && m->ifm_current==0x80 && m->ifm_active==0x80);
  /* This is IPConfiguration's real valid/active predicate. */
  assert((m->ifm_count>0 && (m->ifm_status&1)) && !!(m->ifm_status&2)==live);assert(!calls);
  uint64_t address=0x12345678;memcpy(data.b+(size==48?40:36),&address,size==40?4:8);
  m->ifm_count=32;assert(!river_wifi_media_query(cmd,m,live));assert(m->ifm_count==1 && word==0x80 && dest==address && calls==1);calls=0;
  fault=1;m->ifm_count=1;assert(river_wifi_media_query(cmd,m,live)==EFAULT && calls==1);calls=0;fault=0;
  m->ifm_count=-1;assert(river_wifi_media_query(cmd,m,live)==EINVAL && !calls);
 }
 assert(river_wifi_media_query(0xc0306938u,NULL,true)==EINVAL);
 puts("PASS media count-only query, valid/inactive distinction, 32/packed64/kernel64 list ABI, bounded copyout and error propagation");return 0;
}
