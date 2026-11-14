#define PAGE_SIZE 16384u
#define PAGE_MASK (PAGE_SIZE-1)
#define KMA_DATA 1
#define KMA_ZERO 2
#define KMA_PERMANENT 4
#define VM_KERN_MEMORY_IOKIT 1
#define KERN_SUCCESS 0
#ifndef EREMOTE
#define EREMOTE 71
#endif
typedef uint64_t addr64_t;
typedef uint64_t ppnum_t;
static int kernel_map,kernel_pmap;
static unsigned cleaned;
static unsigned char pcm_page[PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));
static int kmem_alloc_contig(int map,vm_offset_t *out,unsigned size,unsigned mask,
 unsigned low,unsigned high,unsigned flags,unsigned tag)
{
 assert(!map && size==PAGE_SIZE && mask==PAGE_MASK && !low && !high && flags==7 && tag==1);
 *out=(uintptr_t)pcm_page;memset(pcm_page,0,sizeof(pcm_page));return 0;
}
static ppnum_t pmap_find_phys(int map,addr64_t address)
{assert(!map && address>=(uintptr_t)pcm_page && address<(uintptr_t)pcm_page+PAGE_SIZE);return 0x60000000/PAGE_SIZE;}
#define ptoa_64(n) ((n)*PAGE_SIZE)
static void sweet_dcache_clean(vm_offset_t address,unsigned bytes)
{assert(address==(uintptr_t)pcm_page && bytes==PAGE_SIZE);cleaned++;}
