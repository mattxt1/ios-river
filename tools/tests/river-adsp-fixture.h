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
typedef CC_SHA256_CTX SHA256_CTX;
static void SHA256_Init(SHA256_CTX *ctx) {assert(CC_SHA256_Init(ctx));}
static void SHA256_Update(SHA256_CTX *ctx,const void *data,size_t length) {
 assert(length==7516||length==0x4000);assert(CC_SHA256_Update(ctx,data,(CC_LONG)length));
}
static void SHA256_Final(void *out,SHA256_CTX *ctx) {assert(CC_SHA256_Final(out,ctx));}
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
 ri_adsp_metadata_attempted=ri_adsp_metadata_accepted=false;ri_adsp_metadata_page=0;
}
