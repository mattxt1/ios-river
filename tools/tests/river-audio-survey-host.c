/* Exercise the actual added survey with MMIO/SPMI substitutes. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
typedef uintptr_t vm_offset_t;
struct request { void *newptr; char out[2048]; };
#define SYSCTL_HANDLER_ARGS (struct request *req)
#define LCK_GRP_DECLARE(name, label) int name __attribute__((unused))
#define LCK_MTX_DECLARE(name, group) int name
#define SYSCTL_PROC(...)
static void lck_mtx_lock(int *p) { assert(!*p); *p = 1; }
static void lck_mtx_unlock(int *p) { assert(*p); *p = 0; }
static int output(struct request *r, const void *s, size_t n) {
    assert(n <= sizeof(r->out)); memcpy(r->out, s, n); return 0;
}
#define SYSCTL_OUT output
static int map_count, spmi_count, mode;
static vm_offset_t ri_map(vm_offset_t pa) {
    map_count++;
    assert(pa == 0x0200f000 || pa == 0x0180b008 || pa == 0x0180f01c ||
           pa == 0x0102e000 || pa == 0x01044000 ||
           (pa >= 0x01087000 && pa <= 0x0108a000));
    return mode == 1 ? 0 : pa;
}
static uint32_t ri_rd(vm_offset_t p, unsigned off) {
    assert(p); /* Never dereference a failed mapping. */
    if (p == 0x0200f000) { assert(off == 0); return mode == 2 ? 0 : 0x20010000; }
    assert(p != 0x07af7000 && p < 0x0c000000); /* No QUP7 or LPASS access. */
    assert(off == 0 || off == 4 || (p == 0x0180f01c && off == 8));
    return 0;
}
static vm_offset_t ri_spmi_obs(vm_offset_t core, unsigned ppid) {
    assert(core == 0x0200f000);
    assert(ppid == 0x145 || ppid == 0x11d || ppid == 0xc5);
    return mode == 3 ? 0 : ppid;
}
static bool ri_spmi_read(vm_offset_t obs, unsigned reg, unsigned n, uint32_t *v) {
    assert(obs); assert(n == 1); assert(reg == 4 || reg == 5 || reg == 8 || reg == 0x46);
    spmi_count++; if (mode == 4) return false; *v = 0; return true;
}
#include "audio-survey-added.h"
int main(void) {
    struct request r = { .newptr = (void *)1 };
    assert(river_audio_prereq_sysctl(&r) == EPERM); assert(!map_count && !spmi_count);
    r.newptr = NULL;
    mode = 1; assert(!river_audio_prereq_sysctl(&r));
    assert(strstr(r.out, "spmi_version=0")); assert(!spmi_count);
    mode = 2; assert(!river_audio_prereq_sysctl(&r)); assert(!spmi_count);
    mode = 3; assert(!river_audio_prereq_sysctl(&r)); assert(strstr(r.out, "readable=0"));
    mode = 4; assert(!river_audio_prereq_sysctl(&r)); assert(strstr(r.out, "readable=0"));
    mode = 0; assert(!river_audio_prereq_sysctl(&r));
    assert(strstr(r.out, "capture=synthetic")); assert(strstr(r.out, "readable=1"));
    int maps = map_count;
    for (int i = 0; i < 10; i++) assert(!river_audio_prereq_sysctl(&r));
    assert(map_count == maps); /* Repeated polls cannot leak MMIO mappings. */
    puts("PASS: survey write rejection, bounded mappings, unreadable rails, unsupported SPMI, allowed MMIO only");
}
