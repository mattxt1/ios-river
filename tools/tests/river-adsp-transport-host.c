#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
typedef uintptr_t vm_offset_t;
static uint32_t mem[0x100000/4], before[0x100000/4];
static char ri_adsp_note[8192];
static unsigned reads,mutate_at,end_reads;
static uint32_t ri_rd(vm_offset_t base,unsigned offset)
{
 assert(base==(uintptr_t)mem && !(offset&3) && offset<sizeof(mem));reads++;
 uint32_t result=mem[offset/4];
 if(offset==0x8600c && ++end_reads==mutate_at)mem[offset/4]+=8;
 return result;
}
static void host_barrier(void) { }
#include "adsp-transport-actual.h"
static void reset(void)
{
 memset(mem,0,sizeof(mem));strcpy(ri_adsp_note,"prior-note");reads=mutate_at=end_reads=0;
 mem[0xff000/4]=0x434f5424;mem[0xff004/4]=1;mem[0xff008/4]=7;
 mem[0xff080/4]=0x86000;mem[0xff084/4]=0x24000;mem[0xff08c/4]=0x20000;mem[0xff090/4]=32;
 mem[0x86000/4]=0x54525024;mem[0x86004/4]=0x20000;mem[0x86008/4]=0x24000;
 mem[0x8600c/4]=32;mem[0x86010/4]=0x24000;
}
static unsigned alloc(unsigned id,unsigned bytes,unsigned pad_header)
{
 unsigned cursor=mem[0x8600c/4], h=0x86000+cursor;
 unsigned padded=(bytes+7)&~7u;
 assert(cursor+16+pad_header+padded<=0x24000);
 mem[h/4]=(id<<16)|0xa5a5;mem[(h+4)/4]=padded;
 mem[(h+8)/4]=(pad_header<<16)|(padded-bytes);
 mem[0x8600c/4]=cursor+16+pad_header+padded;return h+16+pad_header;
}
static void channel(unsigned table,unsigned slot,const char *name,unsigned cid)
{
 unsigned e=table+slot*32;
 assert(slot<64 && strlen(name)<20);
 memcpy((char *)mem+e,name,strlen(name)+1);mem[(e+20)/4]=cid;
 mem[(e+24)/4]=1;mem[(e+28)/4]=1;
}
static void survey(void)
{
 memcpy(before,mem,sizeof(mem));ri_adsp_transport_survey((uintptr_t)mem);
 assert(!memcmp(before,mem,sizeof(mem)));assert(reads<20000);
 assert(memchr(ri_adsp_note,0,sizeof(ri_adsp_note)));
}
static void contains(const char *text) {assert(strstr(ri_adsp_note,text));}
int main(void)
{
 struct ri_adsp_transport_records r;
 reset();survey();contains("private_transport_records_valid=1 records=0");
 contains("private_channels=0");contains("private_transport_snapshot_stable=1");
 reset();unsigned table=alloc(13,2048,0),info=alloc(17,88,0),fifo=alloc(341,2048,0);
 channel(table,0,"apr_audio_svc",3);mem[info/4]=0;mem[(info+44)/4]=2;
 mem[(info+36)/4]=4;mem[(info+40)/4]=12;mem[(info+80)/4]=20;mem[(info+84)/4]=100;
 unsigned d=alloc(478,32,0);alloc(479,16384,0);alloc(480,16384,0);
 mem[d/4]=8;mem[(d+4)/4]=16;mem[(d+8)/4]=24;mem[(d+12)/4]=32;
 assert(ri_adsp_transport_collect((uintptr_t)mem,&r));assert(r.info[3].offset==info && r.fifo[3].offset==fifo);
 survey();contains("name=apr_audio_svc cid=3");contains("layout_valid=1 states=0,2");
 contains("tx_head_tail=0xc,0x4 rx_head_tail=0x64,0x14 indices_in_range=1");
 contains("private_channels=1 private_apr_audio_names=1");contains("in_range_aligned=1");
 mem[(info+84)/4]=1024;strcpy(ri_adsp_note,"");survey();contains("indices_in_range=0");
 mem[(d+12)/4]=16384;strcpy(ri_adsp_note,"");survey();contains("in_range_aligned=0");
 reset();table=alloc(13,2048,0);info=alloc(18,40,8);alloc(342,4096,0);
 channel(table,0,"apr_audio_svc",4);mem[info/4]=1;mem[(info+20)/4]=1;
 mem[(info+16)/4]=100;mem[(info+12)/4]=20;mem[(info+36)/4]=200;mem[(info+32)/4]=40;
 survey();contains("layout_valid=1 states=1,1");contains("indices_in_range=1");
 reset();table=alloc(13,2048,0);channel(table,0,"bad-cid",64);survey();contains("entry_valid=0");
 reset();table=alloc(13,2048,0);memset((char *)mem+table,'x',20);mem[(table+28)/4]=1;
 survey();contains("entry_valid=0");
 reset();table=alloc(13,2048,0);channel(table,0,"a\nb",3);survey();contains("name=a?b");contains("layout_valid=0");
 reset();table=alloc(266,2048,0);channel(table,0,"bridge",3);survey();contains("bridge_layout_not_decoded=1");
 reset();alloc(13,2040,0);survey();contains("bytes=2040");contains("private_channels=0");
 reset();alloc(13,2048,0);alloc(13,2048,0);survey();contains("private_transport_records_valid=0");
 reset();table=alloc(13,2048,0);for(unsigned i=0;i<64;i++)channel(table,i,"apr_audio_svc",i);
 survey();contains("private_transport_output_truncated=1");assert(strlen(ri_adsp_note)<sizeof(ri_adsp_note)-1);
 reset();memset(ri_adsp_note,'z',sizeof(ri_adsp_note)-80);ri_adsp_note[sizeof(ri_adsp_note)-80]=0;
 survey();contains("private_transport_output_truncated=1");
 const unsigned bad[]={0xff000,0xff004,0xff008,0xff080,0xff084,0xff088,0xff08c,0xff090,
  0x86000,0x86004,0x86008,0x8600c,0x86010};
 for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++) {
  reset();mem[bad[i]/4]^=1;assert(!ri_adsp_transport_collect((uintptr_t)mem,&r));
 }
 for(unsigned field=0;field<3;field++) {
  reset();alloc(13,2048,0);mem[(0x86020+field*4)/4]=0xffffffff;
  assert(!ri_adsp_transport_collect((uintptr_t)mem,&r));
 }
 reset();alloc(13,2048,0);mutate_at=1;assert(!ri_adsp_transport_collect((uintptr_t)mem,&r));
 reset();alloc(13,2048,0);mutate_at=2;ri_adsp_transport_survey((uintptr_t)mem);
 contains("private_transport_snapshot_stable=0");
 /* Unknown records are bounded and skipped; too many records cannot loop. */
 reset();for(unsigned i=0;i<513;i++)alloc(600+i,8,0);
 assert(!ri_adsp_transport_collect((uintptr_t)mem,&r));assert(r.count==512);
 reset();alloc(65000,8,0);survey();contains("private_transport_records_valid=1 records=1");
 puts("actual private ADSP transport survey: SMD40/88, GLINK indices, padded/unknown/duplicate/malformed records, races, names/CIDs, bounded output and zero memory writes PASS");
}
