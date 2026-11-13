#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
typedef uintptr_t vm_offset_t;
#define LCK_GRP_DECLARE(a,b) int a __attribute__((unused))
#define LCK_MTX_DECLARE(a,b) int a __attribute__((unused))
static uint32_t smem[0x100000/4];
static vm_offset_t river_smd_smem;
static unsigned scm_calls,scenario;
static int64_t river_scm(uint32_t svc,uint32_t cmd,uint64_t ai,uint64_t a0,uint64_t a1,uint64_t a2,uint64_t *r) {
#ifdef RI_ADSP_STAGE_HOST
 if(svc==2&&cmd==2) return stage_scm(ai,a0,a1,a2,r);
#endif
#ifdef RI_ADSP_METADATA_HOST
 if(svc==2&&cmd==1) return metadata_scm(ai,a0,a1,a2,r);
#endif
 assert(ai==1&&!a1&&!a2);scm_calls++;
 if(svc==6&&cmd==1) assert(a0==0x42000201||a0==0x42000202||a0==0x42000205);
 else assert(svc==2&&cmd==7&&a0==1);
 *r=scenario==1?0:1;return scenario==2?-1:0;
}
static uint32_t ri_rd(vm_offset_t base,unsigned off) {
 assert(base==(uintptr_t)smem && !(off&3) && off<sizeof(smem));
 return smem[off/4];
}
#include "river-adsp-added.h"
static void word(unsigned off,uint32_t v) {assert(!(off&3)&&off<sizeof(smem));smem[off/4]=v;}
static void item(unsigned id,unsigned off,unsigned len,unsigned aux) {
 unsigned e=0xd0+id*16;word(e,1);word(e+4,off);word(e+8,len);word(e+12,aux);
}
static void reset(void) {
 memset(smem,0,sizeof(smem));river_smd_smem=(uintptr_t)smem;scm_calls=scenario=0;
 word(0xc0,1);word(0xc4,0x8000);word(0xc8,sizeof(smem)-0x8000);
 item(13,0x4000,64*32,0);item(15,0x5000,88,0);
 memcpy((char *)smem+0x4000,"apr_audio_svc",14);
 word(0x4000+20,1);word(0x4000+24,1);word(0x4000+28,1);
 word(0x5000,2);word(0x5000+44,1);
}
int main(void) {
 reset();uint32_t before[sizeof(smem)/4];memcpy(before,smem,sizeof(smem));
 assert(!ri_adsp_probe_impl());assert(scm_calls==4);assert(!memcmp(before,smem,sizeof(smem)));
 assert(strstr(ri_adsp_note,"adsp_channels=1 apr_channels=1"));
 assert(strstr(ri_adsp_note,"states_read=1 tx=2 rx=1"));
 assert(strstr(ri_adsp_note,"pcm_transport=0"));
 reset();river_smd_smem=0;assert(!ri_adsp_probe_impl());assert(strstr(ri_adsp_note,"smem=unavailable"));
 reset();scenario=1;assert(!ri_adsp_probe_impl());assert(strstr(ri_adsp_note,"supported=0"));
 reset();scenario=2;assert(ri_adsp_probe_impl()==EIO);assert(strstr(ri_adsp_note,"support_status=-1"));
 reset();word(0xc4,0x100004);assert(ri_adsp_probe_impl()==EIO);assert(strstr(ri_adsp_note,"smem_valid=0"));
 reset();word(0xc8,0xffffffff);assert(ri_adsp_probe_impl()==EIO);
 reset();item(13,0xffff0,2048,0);assert(!ri_adsp_probe_impl());assert(strstr(ri_adsp_note,"legacy_tables=0"));
 reset();item(13,0x4000,2048,0x60000);assert(!ri_adsp_probe_impl());assert(strstr(ri_adsp_note,"legacy_tables=0"));
 reset();word(0x4000+20,200);assert(!ri_adsp_probe_impl());assert(strstr(ri_adsp_note,"malformed=1"));
 reset();memset((char *)smem+0x4000,'X',20);assert(!ri_adsp_probe_impl());assert(strstr(ri_adsp_note,"malformed=1"));
 reset();word(0x4000+24,6);assert(!ri_adsp_probe_impl());assert(strstr(ri_adsp_note,"adsp_channels=0"));
 reset();item(266,0x6000,2048,0);memcpy((char *)smem+0x6000,"apr_other",10);
 word(0x6000+20,1);word(0x6000+24,1);word(0x6000+28,1);
 assert(!ri_adsp_probe_impl());assert(strstr(ri_adsp_note,"states_read=0"));
 assert(strstr(ri_adsp_note,"adsp_channels=2"));
 assert(strlen(ri_adsp_note)<sizeof(ri_adsp_note));
 puts("PASS: ADSP read-only SCM allowlist, retained SMEM bounds, foreign/invalid descriptors and channel states");
 return 0;
}
