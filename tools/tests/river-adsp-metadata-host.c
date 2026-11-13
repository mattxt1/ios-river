#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <CommonCrypto/CommonDigest.h>
static unsigned secure_calls,cleaned,allocations;
static int64_t secure_status;
static uint64_t secure_result;
static int64_t metadata_scm(uint64_t ai,uint64_t pas,uint64_t pa,uint64_t zero,uint64_t *r) {
 assert(ai==(2|(2<<6))&&pas==1&&pa==0x20000000&&!zero&&cleaned);
 secure_calls++;*r=secure_result;return secure_status;
}
#define RI_ADSP_METADATA_HOST 1
#define main existing_probe_tests
#include "river-adsp-probe-host.c"
#undef main
typedef uint32_t ppnum_t;
typedef uintptr_t addr64_t;
#define PAGE_SIZE 16384u
#define PAGE_MASK (PAGE_SIZE-1)
#define KMA_DATA 1
#define KMA_ZERO 2
#define KMA_PERMANENT 4
#define VM_KERN_MEMORY_IOKIT 1
#define KERN_SUCCESS 0
#define ptoa_64(pn) ((uint64_t)(pn)<<14)
static void *kernel_map,*kernel_pmap;
static uint8_t page[PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));
static bool gpu_ready,alloc_fail;
static uint32_t physical;
static bool river_gpu_pas_ready(void) {return gpu_ready;}
static void *ccsha256_di(void) {return NULL;}
static void ccdigest(void *di,size_t len,const void *data,void *out) {
 (void)di;assert(len==7516);CC_SHA256(data,(CC_LONG)len,out);
}
static int kmem_alloc_contig(void *map,vm_offset_t *out,size_t size,unsigned mask,unsigned a,unsigned b,unsigned flags,unsigned tag) {
 assert(map==kernel_map&&size==PAGE_SIZE&&mask==PAGE_MASK&&!a&&!b&&flags==7&&tag==1);
 allocations++;if(alloc_fail)return 1;memset(page,0,sizeof(page));*out=(uintptr_t)page;return 0;
}
static ppnum_t pmap_find_phys(void *map,addr64_t va) {assert(map==kernel_pmap&&va==(uintptr_t)page);return physical>>14;}
static void sweet_dcache_clean(vm_offset_t va,unsigned size) {
 assert(va==(uintptr_t)page&&size==PAGE_SIZE);
 for(unsigned i=0;i<7516;i++)assert(page[i]==(uint8_t)(i*7));
 for(unsigned i=7516;i<PAGE_SIZE;i++)assert(!page[i]);cleaned++;
}
#include "river-adsp-metadata-added.h"
static void setup(void) {
 reset();word(0x4000+28,0);gpu_ready=true;alloc_fail=false;physical=0x20000000;
 secure_calls=cleaned=allocations=0;secure_status=0;secure_result=0;
 ri_adsp_metadata_attempted=false;ri_adsp_metadata_page=0;
}
int main(void) {
 uint8_t data[7516];for(unsigned i=0;i<sizeof(data);i++)data[i]=(uint8_t)(i*7);
 setup();assert(!ri_adsp_metadata_auth(data,sizeof(data)));assert(secure_calls==1&&allocations==1&&cleaned==1&&scm_calls==4);
 assert(ri_adsp_metadata_attempted&&strstr(ri_adsp_note,"metadata_accepted=1"));
 assert(strstr(ri_adsp_note,"firmware_start_requested=0 firmware_memory_written=0"));
 assert(ri_adsp_metadata_auth(data,sizeof(data))==EALREADY&&secure_calls==1&&allocations==1&&scm_calls==4);
 setup();data[0]^=1;assert(ri_adsp_metadata_auth(data,sizeof(data))==EINVAL);data[0]^=1;
 assert(!secure_calls&&!allocations&&!scm_calls);
 setup();assert(ri_adsp_metadata_auth(data,sizeof(data)-1)==EINVAL&&!scm_calls);
 setup();gpu_ready=false;assert(ri_adsp_metadata_auth(data,sizeof(data))==EBUSY&&!allocations&&!scm_calls);
 setup();scenario=1;assert(ri_adsp_metadata_auth(data,sizeof(data))==EBUSY&&!allocations&&!secure_calls);
 setup();river_smd_smem=0;assert(ri_adsp_metadata_auth(data,sizeof(data))==EBUSY&&!allocations&&!secure_calls);
 setup();word(0x4000+28,1);assert(ri_adsp_metadata_auth(data,sizeof(data))==EBUSY&&!allocations&&!secure_calls);
 setup();alloc_fail=true;assert(ri_adsp_metadata_auth(data,sizeof(data))==ENOMEM&&!secure_calls);
 setup();physical=0x8d600000;assert(ri_adsp_metadata_auth(data,sizeof(data))==EIO&&!secure_calls&&!cleaned);
 setup();secure_status=-1;assert(ri_adsp_metadata_auth(data,sizeof(data))==EIO&&secure_calls==1&&ri_adsp_metadata_attempted);
 assert(ri_adsp_metadata_auth(data,sizeof(data))==EALREADY&&secure_calls==1);
 setup();secure_result=1;assert(ri_adsp_metadata_auth(data,sizeof(data))==EIO&&ri_adsp_metadata_attempted);
 puts("PASS: stock metadata digest, crypto/PAS/channel guards, kernel-RAM bounds, cache clean, only INIT_IMAGE and retained one-shot on failure");
}
