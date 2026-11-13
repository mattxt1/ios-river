#include <assert.h>
#include <stdint.h>
static unsigned mem_calls,maps,unmaps,barriers,map_fail;
static int64_t mem_status;
static uint64_t mem_result;
static int64_t stage_scm(uint64_t ai,uint64_t pas,uint64_t base,uint64_t size,uint64_t *r) {
 assert(ai==3&&pas==1&&base==0x8d600000&&size==0x1100000);
 mem_calls++;*r=mem_result;return mem_status;
}
#define RI_ADSP_STAGE_HOST 1
#include "river-adsp-fixture.h"
#define VM_WIMG_WCOMB 1
static uint8_t image[0x1100000] __attribute__((aligned(16384)));
static vm_offset_t ml_io_map_unmappable(vm_offset_t pa,size_t length,unsigned flags) {
 assert(pa>=0x8d600000&&pa<=0x8e700000-0x4000&&!(pa&0x3fff)&&length==0x4000&&flags==VM_WIMG_WCOMB);
 assert(ri_adsp_metadata_accepted&&mem_calls==1);
 maps++;if(maps==map_fail)return 0;
 return (uintptr_t)(image+pa-0x8d600000);
}
static void host_stage_dsb(void) {barriers++;}
static void ml_io_unmap(vm_offset_t p,size_t len) {
 assert(p>=(uintptr_t)image&&p<=(uintptr_t)(image+sizeof(image)-0x4000)&&len==0x4000);
 unmaps++;assert(barriers==unmaps);
}
static void ri_wait(unsigned ms) {assert(ms==100);}
#define printf(...) ((void)0)
#include "river-adsp-stage-added.h"
#undef printf
static uint8_t chunk[0x4000];
static void ready(void) {
 setup();ri_adsp_metadata_accepted=true;
 ri_adsp_stage_attempted=ri_adsp_stage_open=ri_adsp_stage_failed=ri_adsp_image_staged=false;
 ri_adsp_stage_offset=0;mem_calls=maps=unmaps=barriers=map_fail=0;mem_status=0;mem_result=0;
 memset(image,0,sizeof(image));for(unsigned i=0;i<sizeof(chunk);i++)chunk[i]=(uint8_t)(i*7);
}
static void full_image(bool bad) {
 assert(!ri_adsp_stage_request(RI_ADSP_STAGE_BEGIN,0,NULL,0));
 for(unsigned off=0;off<sizeof(image);off+=sizeof(chunk)) {
  if(bad&&!off)chunk[0]^=1;
  assert(!ri_adsp_stage_request(RI_ADSP_STAGE_DATA,off,chunk,sizeof(chunk)));
  if(bad&&!off)chunk[0]^=1;
 }
}
int main(void) {
 ready();ri_adsp_metadata_accepted=false;
 assert(ri_adsp_stage_request(RI_ADSP_STAGE_BEGIN,0,NULL,0)==EBUSY&&!mem_calls&&!maps);
 ready();assert(ri_adsp_stage_request(RI_ADSP_STAGE_BEGIN,4,NULL,0)==EINVAL&&!mem_calls);
 ready();mem_status=-1;assert(ri_adsp_stage_request(RI_ADSP_STAGE_BEGIN,0,NULL,0)==EIO&&!maps);
 assert(ri_adsp_stage_request(RI_ADSP_STAGE_BEGIN,0,NULL,0)==EALREADY);
 ready();full_image(false);assert(!ri_adsp_stage_request(RI_ADSP_STAGE_END,0,NULL,0));
 assert(ri_adsp_image_staged&&ri_adsp_stage_offset==sizeof(image)&&maps==1088&&unmaps==maps&&barriers==maps);
 for(unsigned i=0;i<sizeof(image);i++)assert(image[i]==(uint8_t)(i*7));
 assert(strstr(ri_adsp_note,"firmware_start_requested=0 executable_readback=not_performed"));
 assert(ri_adsp_stage_request(RI_ADSP_STAGE_DATA,0,chunk,sizeof(chunk))==EBUSY&&maps==1088);
 ready();full_image(true);assert(ri_adsp_stage_request(RI_ADSP_STAGE_END,0,NULL,0)==EIO&&ri_adsp_stage_failed&&!ri_adsp_image_staged);
 ready();assert(!ri_adsp_stage_request(RI_ADSP_STAGE_BEGIN,0,NULL,0));
 assert(ri_adsp_stage_request(RI_ADSP_STAGE_DATA,0x4000,chunk,sizeof(chunk))==EINVAL&&!maps);
 assert(ri_adsp_stage_request(RI_ADSP_STAGE_DATA,0,chunk,sizeof(chunk)-1)==EINVAL&&!maps);
 assert(ri_adsp_stage_request(RI_ADSP_STAGE_DATA,0x1100000,chunk,sizeof(chunk))==EINVAL&&!maps);
 assert(ri_adsp_stage_request(RI_ADSP_STAGE_END,0,NULL,0)==EINVAL);
 map_fail=1;assert(ri_adsp_stage_request(RI_ADSP_STAGE_DATA,0,chunk,sizeof(chunk))==ENOMEM&&ri_adsp_stage_failed&&!unmaps&&!barriers);
 assert(ri_adsp_stage_request(RI_ADSP_STAGE_DATA,0,chunk,sizeof(chunk))==EBUSY&&maps==1);
 puts("PASS: exact PAS1 memory setup, no-start staging, ordered/range-bounded 1088 pages, hash mismatch and map-failure interlocks");
}
