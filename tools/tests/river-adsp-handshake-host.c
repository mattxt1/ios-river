#include <stdint.h>
#include <assert.h>
static uint32_t lock_owner;
static unsigned writes,lock_writes,release_writes,maps,unmaps,spins,barriers,test_case;
static uint32_t handshake_lock_read(unsigned off) {assert(!off);return lock_owner;}
#define RI_ADSP_HANDSHAKE_HOST 1
#define main existing_probe_tests
#include "river-adsp-probe-host.c"
#undef main
#define VM_WIMG_IO 1
static vm_offset_t ml_io_map_unmappable(vm_offset_t pa,size_t size,unsigned flags) {
 assert(pa==0x01908000&&size==0x4000&&flags==1);maps++;return test_case==1?0:pa;
}
static void ml_io_unmap(vm_offset_t pa,size_t size) {assert(pa==0x01908000&&size==0x4000);unmaps++;}
static void delay(unsigned us) {assert(us==10);spins++;assert(spins<=1000);}
static void host_handshake_barrier(void) {barriers++;}
static void ri_wr(vm_offset_t base,unsigned off,uint32_t value) {
 if(base==0x01908000) {
  assert(!off);lock_writes++;
  if(value==1) {
   if(!lock_owner)lock_owner=1;
   if(test_case==3)word(0x8600c,40);
  } else {assert(!value&&lock_owner==1);lock_owner=0;release_writes++;}
  return;
 }
 assert(base==(uintptr_t)smem&&lock_owner==1);
 assert(off==0x8600c||(off>=0x86020&&off<0x86188&&!(off&3)));
 if(off==0x8600c)assert(value==0x188&&ri_rd(base,0x86034)==1&&barriers>=2);
 writes++;word(off,value);
 if(test_case==4&&off==0x8600c)word(0x86030,0); /* failed final record readback */
}
#include "river-adsp-handshake-added.h"
static void ready(void) {
 reset();word(0xff000,0x434f5424);word(0xff004,1);word(0xff008,7);
 word(0xff080,0x86000);word(0xff084,0x24000);word(0xff08c,0x20000);word(0xff090,32);
 word(0x86000,0x54525024);word(0x86004,0x20000);word(0x86008,0x24000);
 word(0x8600c,32);word(0x86010,0x24000);word(0x4000+28,0);
 lock_owner=0;writes=lock_writes=release_writes=maps=unmaps=spins=barriers=test_case=0;
 ri_adsp_handshake_attempted=ri_adsp_handshake_created=false;
}
int main(void) {
 ready();uint32_t before[sizeof(smem)/4];memcpy(before,smem,sizeof(smem));
 assert(!ri_adsp_handshake_impl());assert(ri_adsp_handshake_created&&ri_adsp_handshake_attempted);
 assert(maps==1&&unmaps==1&&release_writes==1&&!lock_owner&&writes);
 assert(!memcmp(before,smem,0x8600c));assert(!memcmp((char *)before+0x86010,(char *)smem+0x86010,16));
 assert(!memcmp((char *)before+0x86188,(char *)smem+0x86188,sizeof(smem)-0x86188));
 assert(ri_rd((uintptr_t)smem,0x86028)==4&&ri_rd((uintptr_t)smem,0x8600c)==0x188);
 assert(strstr(ri_adsp_note,"ipc_sent=0 firmware_start_requested=0 pcm_transport=0"));
 unsigned old=writes;assert(ri_adsp_handshake_impl()==EALREADY&&writes==old&&maps==1);
 assert(!ri_adsp_probe_impl());assert(strstr(ri_adsp_note,"partition_item=429")&&strstr(ri_adsp_note,"smp2p_valid=1"));
 ready();word(0x8600c,40);assert(ri_adsp_handshake_impl()==EBUSY&&!maps&&!writes);
 ready();test_case=1;assert(ri_adsp_handshake_impl()==ENOMEM&&!writes&&!lock_writes);
 ready();lock_owner=1;assert(ri_adsp_handshake_impl()==EBUSY&&!writes&&!lock_writes&&unmaps==1);
 ready();lock_owner=0xffffffff;assert(ri_adsp_handshake_impl()==EBUSY&&!lock_writes);
 ready();lock_owner=2;assert(ri_adsp_handshake_impl()==ETIMEDOUT&&spins==1000&&!writes&&!release_writes&&lock_owner==2);
 ready();test_case=3;assert(ri_adsp_handshake_impl()==EBUSY&&!writes&&release_writes==1&&!lock_owner);
 ready();test_case=4;assert(ri_adsp_handshake_impl()==EIO&&release_writes==1&&!ri_adsp_handshake_created&&ri_adsp_handshake_attempted);
 assert(ri_rd((uintptr_t)smem,0x8600c)==0x188); /* Never rewind a published shared heap. */
 puts("PASS: S:3 owner exclusion, bounded acquisition, empty partition recheck, publish ordering, exact writes/readback and no IPC/start");
}
