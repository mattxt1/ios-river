/* Run the production identity probe against a mode-0 SPI pin model. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
typedef uintptr_t vm_offset_t;
#define LCK_GRP_DECLARE(name, label) int name __attribute__((unused))
#define LCK_MTX_DECLARE(name, group) int name __attribute__((unused))
static bool ri_speaker_ok;
static unsigned mode, writes, frames, bits, delays, rail_reads;
static uint32_t pin_cfg[4], pin_out[4], saved_out[4];
static unsigned char tx[8];
static unsigned reply_bit;
static vm_offset_t ri_map(vm_offset_t pa) {
    assert(pa == 0x0200f000 || pa == 0x01044000 || pa == 0x0180f01c ||
           (pa >= 0x01087000 && pa <= 0x0108a000));
    if (mode == 7 && pa == 0x01089000) return 0;
    return pa;
}
static void delay(unsigned n) { delays += n; assert(delays < 5000); }
static uint32_t ri_rd(vm_offset_t p, unsigned off) {
    if (p == 0x0200f000) { assert(!off); return mode == 1 ? 0 : 0x20010000; }
    if (p == 0x01044000) return off ? (mode == 4 ? 0 : 3) : 0x203;
    if (p == 0x0180f01c) { assert(off == 0 || off == 8); return mode == 5 ? 1 : 0x80000000; }
    assert(p >= 0x01087000 && p <= 0x0108a000 && !(p & 0xfff));
    unsigned i = (unsigned)(p - 0x01087000) / 0x1000;
    assert(off == 0 || off == 4);
    if (!off) return pin_cfg[i];
    return pin_out[i] | (i == 3 ? reply_bit : 0);
}
static void ri_wr(vm_offset_t p, unsigned off, uint32_t value) {
    assert(p >= 0x01087000 && p <= 0x0108a000 && !(p & 0xfff));
    unsigned i = (unsigned)(p - 0x01087000) / 0x1000;
    assert(off == 0 || off == 4); writes++;
    if (!off) {
        assert(value == 0 || value == 1 || (i != 3 && value == 0x200));
        pin_cfg[i] = value; return;
    }
    assert(value == 0 || value == 2);
    if (i == 1 && pin_cfg[1] == 0x200) {
        if (value == 0 && pin_out[1] == 2) { bits = 0; memset(tx, 0, 8); }
        if (value == 2 && pin_out[1] == 0) {
            assert(bits == 64); assert(frames < 3);
            assert(tx[0] == 0x80 && tx[1] == 0 && tx[2] == 0);
            assert(tx[3] == (frames == 2 ? 1 : 0));
            assert(tx[4] == 0 && tx[5] == 0 && tx[6] == 0 && tx[7] == 0);
            frames++;
        }
    }
    if (i == 0 && pin_cfg[0] == 0x200 && pin_cfg[1] == 0x200 && pin_out[1] == 0 && value == 2) {
        assert(bits < 64); tx[bits / 8] = (unsigned char)((tx[bits / 8] << 1) | (pin_out[2] >> 1));
        unsigned response = frames == 2 ? 3 : 0x6360;
        if (mode == 8) response = 0;
        if (mode == 9) response = 0xffff;
        if (mode == 10 && frames == 1) response = 0x6338;
        reply_bit = bits < 48 ? 0 : (response >> (63 - bits)) & 1;
        bits++;
    }
    pin_out[i] = value;
}
static vm_offset_t ri_spmi_obs(vm_offset_t core, unsigned ppid) {
    assert(core == 0x0200f000); assert(ppid == 0x145 || ppid == 0x11d);
    return mode == 2 ? 0 : ppid;
}
static bool ri_spmi_read(vm_offset_t obs, unsigned reg, unsigned n, uint32_t *v) {
    assert(obs == 0x145 || obs == 0x11d); assert(n == 1); rail_reads++;
    if (mode == 3) return false;
    switch (reg) {
    case 4: *v = obs == 0x145 ? 0x21 : 3; break;
    case 5: *v = obs == 0x145 ? 0x2a : 0xa; break;
    case 8: *v = mode == 11 ? 0 : 0x80; break;
    case 0x46: *v = 0x80; break;
    default: assert(0);
    }
    return true;
}
#include "codec-probe-added.h"
static void reset(unsigned scenario) {
    mode = scenario; writes = frames = bits = delays = rail_reads = reply_bit = 0;
    ri_speaker_ok = true; ri_codec_attempted = 0;
    ri_codec_core = ri_codec_reset = ri_codec_clock = 0;
    memset(ri_codec_pin, 0, sizeof(ri_codec_pin)); memset(ri_codec_rail, 0, sizeof(ri_codec_rail));
    for (unsigned i = 0; i < 4; i++) { pin_cfg[i] = 1; pin_out[i] = saved_out[i] = (i % 2) * 2; }
    if (scenario == 6) pin_cfg[2] = 4;
}
int main(void) {
    reset(0); ri_speaker_ok = false; assert(ri_codec_probe_once() == EBUSY); assert(!writes);
    for (unsigned scenario = 1; scenario <= 7; scenario++) {
        reset(scenario); assert(ri_codec_probe_once()); assert(!writes && !ri_codec_attempted);
    }
    reset(11); assert(ri_codec_probe_once()); assert(!writes);
    for (unsigned scenario = 0; scenario <= 10; scenario++) {
        if (scenario > 0 && scenario < 8) continue;
        reset(scenario);
        assert((ri_codec_probe_once() == 0) == (scenario == 0));
        assert(ri_codec_attempted && frames == 3 && rail_reads == 16);
        for (unsigned i = 0; i < 4; i++) assert(pin_cfg[i] == 1 && pin_out[i] == saved_out[i]);
        assert(strstr(ri_codec_note, "pins_restored=1"));
        unsigned old_writes = writes;
        assert(ri_codec_probe_once() == EALREADY); assert(writes == old_writes);
    }
    puts("PASS: codec prerequisites, exact read-only SPI framing, repeat ID rejection, pin restoration, one-shot limit");
}
