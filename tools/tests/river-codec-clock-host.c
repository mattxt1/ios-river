/* GPIO SPI model: validate actual register framing, bounds and rollback. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
typedef uintptr_t vm_offset_t;
#define LCK_GRP_DECLARE(name, label) int name __attribute__((unused))
#define LCK_MTX_DECLARE(name, group) int name __attribute__((unused))
static bool ri_speaker_ok = true;
static uint16_t regs[0x2000], initial[0x2000];
static uint32_t cfg[4], out[4];
static unsigned mode, writes, ignore_write, bits, reply, elapsed, enabled_once;
static uint8_t tx[8];
static vm_offset_t ri_map(vm_offset_t pa) {
 assert(pa == 0x0200f000 || pa == 0x01044000 || pa == 0x0180f01c ||
        (pa >= 0x01087000 && pa <= 0x0108a000)); return pa;
}
static void delay(unsigned n) { elapsed += n; assert(elapsed < 1000000); }
static void ri_wait(unsigned ms) { delay(ms * 1000); }
static uint32_t ri_rd(vm_offset_t p, unsigned off) {
 if (p == 0x0200f000) return 0x20010000;
 if (p == 0x01044000) return off ? 3 : 0x203;
 if (p == 0x0180f01c) return 0x80000000;
 assert(p >= 0x01087000 && p <= 0x0108a000 && !(p & 0xfff));
 unsigned i = (unsigned)(p - 0x01087000) / 0x1000;
 assert(off == 0 || off == 4); return off ? out[i] | (i == 3 ? reply : 0) : cfg[i];
}
static bool allowed_write(unsigned reg) {
 return reg == 0x171 || reg == 0x172 || reg == 0x173 || reg == 0x174 || reg == 0x175 ||
        reg == 0x176 || reg == 0x179 || reg == 0x17a || reg == 0x185;
}
static uint16_t reg_read(unsigned reg) {
 assert(reg < 0x2000);
 if (mode == 1 && reg == 0) return 0;
 if (reg == 0x1881) return (regs[0x171] & 1) && mode != 20 ? 0x100 : 0;
 return regs[reg];
}
static void ri_wr(vm_offset_t p, unsigned off, uint32_t v) {
 assert(p >= 0x01087000 && p <= 0x0108a000 && !(p & 0xfff));
 unsigned i = (unsigned)(p - 0x01087000) / 0x1000;
 assert(off == 0 || off == 4);
 if (!off) { assert(v == 0 || v == 1 || (i != 3 && v == 0x200)); cfg[i] = v; return; }
 assert(v == 0 || v == 2);
 if (i == 1 && cfg[1] == 0x200) {
  if (v == 0 && out[1] == 2) { bits = 0; memset(tx, 0, 8); }
  if (v == 2 && out[1] == 0) {
   assert(bits == 64 && tx[1] == 0 && tx[4] == 0 && tx[5] == 0);
   unsigned reg = (tx[2] << 8) | tx[3];
   if (tx[0] == 0) {
    assert(allowed_write(reg)); writes++;
    uint16_t value = (uint16_t)((tx[6] << 8) | tx[7]);
    if (writes != ignore_write) {
     if (reg == 0x171 && mode == 25 && enabled_once) value |= 1; /* stuck enabled */
     if (reg == 0x171 && (value & 1)) {
      enabled_once = 1;
      assert(regs[0x173] == 0 && regs[0x174] == 0);
      assert((regs[0x175] & 0xf00) == 0x200 && (regs[0x176] & 0xcf) == 1);
      assert((regs[0x179] & 0x3c) == 8 && (regs[0x17a] & 0xf800) == 0x2800);
      assert((regs[0x172] & 0x3ff) == 1000);
     }
     regs[reg] = reg == 0x172 ? value & 0x7fff : value;
    }
   } else assert(tx[0] == 0x80 && tx[6] == 0 && tx[7] == 0);
  }
 }
 if (i == 0 && cfg[0] == 0x200 && cfg[1] == 0x200 && out[1] == 0 && v == 2) {
  assert(bits < 64); tx[bits / 8] = (uint8_t)((tx[bits / 8] << 1) | (out[2] >> 1));
  unsigned reg = (tx[2] << 8) | tx[3];
  uint16_t value = bits >= 48 && tx[0] == 0x80 ? reg_read(reg) : 0;
  reply = bits < 48 ? 0 : (value >> (63 - bits)) & 1; bits++;
 }
 out[i] = v;
}
static vm_offset_t ri_spmi_obs(vm_offset_t core, unsigned ppid) {
 assert(core == 0x0200f000 && (ppid == 0x145 || ppid == 0x11d)); return ppid;
}
static bool ri_spmi_read(vm_offset_t obs, unsigned reg, unsigned n, uint32_t *v) {
 assert(n == 1 && (obs == 0x145 || obs == 0x11d));
 switch (reg) {
 case 4: *v = obs == 0x145 ? 0x21 : 3; break;
 case 5: *v = obs == 0x145 ? 0x2a : 0xa; break;
 case 8: case 0x46: *v = 0x80; break;
 default: assert(0);
 }
 return true;
}
#include "codec-probe-added.h"
#include "codec-clock-added.h"
static void reset(unsigned scenario) {
 mode = scenario; writes = ignore_write = bits = reply = elapsed = enabled_once = 0;
 ri_clock_attempted = ri_clock_surveyed = 0;
 for (unsigned i = 0; i < 4; i++) { cfg[i] = 1; out[i] = 0; }
 memset(regs, 0, sizeof(regs)); regs[0] = 0x6360; regs[1] = 1;
 regs[0x101] = 0x404; regs[0x171] = 2; regs[0x172] = 8;
 regs[0x173] = 24; regs[0x174] = 125; regs[0x185] = 1;
 regs[0x1841] = regs[0x1941] = 0xffff;
 if (scenario == 21) regs[0x101] |= 0x40;
 if (scenario == 22) regs[0x171] |= 1;
 if (scenario == 23) regs[0x1841] &= ~0x100;
 memcpy(initial, regs, sizeof(regs));
}
static void check_pins(void) { for (unsigned i = 0; i < 4; i++) assert(cfg[i] == 1 && out[i] == 0); }
static void check_restore(void) {
 check_pins(); assert(!memcmp(regs, initial, sizeof(regs)));
 assert(strstr(ri_clock_note, "clock_restored=1"));
}
int main(void) {
 reset(0); assert(!ri_clock_run(false)); assert(!writes); check_pins();
 assert(strstr(ri_clock_note, "snapshot=1")); assert(ri_clock_run(false) == EALREADY);
 reset(0); assert(!ri_clock_run(true)); check_restore(); assert(strstr(ri_clock_note, "locked=1"));
 unsigned old_writes = writes; assert(ri_clock_run(true) == EALREADY); assert(writes == old_writes);
 for (unsigned n = 1; n <= 10; n++) {
  reset(0); ignore_write = n;
  int rc = ri_clock_run(true);
  assert(rc || n == 9); /* Sync DFSAT already equals the requested value. */
  check_restore();
 }
 reset(20); assert(ri_clock_run(true) == ETIMEDOUT); check_restore();
 assert(strstr(ri_clock_note, "locked=0"));
 unsigned scenarios[] = {1, 21, 22, 23};
 for (unsigned i = 0; i < 4; i++) {
  reset(scenarios[i]); assert(ri_clock_run(true)); assert(!writes); check_pins();
 }
 reset(25); assert(ri_clock_run(true) == EIO); check_pins();
 assert(strstr(ri_clock_note, "clock_restored=0"));
 assert(!ri_clock_write_allowed(0x101) && !ri_clock_write_allowed(0x500));
 puts("PASS: FLL framing, clock recipe, idle/IRQ gates, timeout, write-failure rollback, stuck-enable detection");
}
