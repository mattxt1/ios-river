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
static unsigned active_n, pmic_writes, pmic_ignore, pmic_mode, pmic_pull, cmd_missing, pmic_reads;
static unsigned mode, writes, ignore_write, bits, reply, elapsed, enabled_once;
static uint8_t tx[8];
static uint32_t amp[0x11000],amp_initial[0x11000];
static unsigned amp_writes,amp_ignore,amp_no_lock,gpio_reads;
static bool codec_outputs_off(void) {
 bool bclk=((regs[0x1710]&0x1ff)==1 && (regs[0x1711]&0x8000)) ||
     (!(regs[0x1710]&0x1ff) && !(regs[0x500]&0x60));
 bool lrclk=((regs[0x1714]&0x1ff)==1 && (regs[0x1715]&0x8000)) ||
     (!(regs[0x1714]&0x1ff) && !(regs[0x501]&3) && !(regs[0x502]&3));
 return bclk && lrclk && !(regs[0x101]&0x40);
}
static bool amp_quiet(void) { return (amp[0x2024]&0x10) && !(amp[0x2018]&1) && !(amp[0x4808]&0x33); }
static bool ri_cs35l41_rd(uint32_t reg,uint32_t *value) {
 assert(reg<0x11000);
 if(reg==0x10098) {
  bool driving=(regs[0x101]&0x40) && (regs[0x500]&0x60)==0x60 &&
      !(regs[0x1710]&0x1ff) && !(regs[0x1714]&0x1ff);
  if(driving) {
   assert(amp_quiet());assert((amp[0x6000]&0x3ff8)==0x2000);assert((amp[0x2c04]&0xff7)==0x370);
   assert((regs[0x500]&0x1f)==11 && (regs[0x506]&0x1fff)==32);
   assert(regs[0x507]==0x1010 && regs[0x508]==0x1010 && (regs[0x504]&7)==2);
   if(regs[0x519]==3) {
    assert(!(regs[0x501]&0x20) && regs[0x5f5]==1 && (regs[0x700]&255)==0x38 && (regs[0x708]&255)==0x38);
    assert(amp[0x4800]==0x10000 && (regs[0x505]&0x1fff)==32 && !(regs[0x509]&63) && (regs[0x50a]&63)==1);
    assert(!(regs[0x1712]&0x1ff));
   } else assert((regs[0x501]&0x20) && !regs[0x519] && !regs[0x51a]);
  }
  *value=driving && !amp_no_lock?0x20000000:0x20000010;return true;
 }
 *value=amp[reg];return true;
}
static bool ri_cs35l41_wr(uint32_t reg,uint32_t value) {
 assert(reg==0x2018||reg==0x2024||reg==0x6000||reg==0x4800||reg==0x4808||
        reg==0x2c04||reg==0x2c0c||reg==0x4840||reg==0x4830||reg==0x4820);
 amp_writes++;if(amp_writes==amp_ignore || (mode==60 && reg==0x4820 && !(value&63)))return false;
 if((reg==0x4808&&(value&0x33))||(reg==0x2024&&!(value&0x10))||(reg==0x2018&&(value&1)))
  assert(codec_outputs_off());
 amp[reg]=value;return true;
}
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
 return (reg>=0x700 && reg<=0x70f) || reg==0x505 || reg==0x509 || reg==0x50a || reg==0x5f5 || reg==0x1712 || reg==0x1713 || reg==0x101||reg==0x102||reg==0x500||reg==0x501||reg==0x502||reg==0x503||
        reg==0x504||reg==0x506||reg==0x507||reg==0x508||reg==0x519||reg==0x51a||
        reg==0x1710||reg==0x1711||reg==0x1714||reg==0x1715||reg == 0x100 || reg == 0x171 || reg == 0x172 || reg == 0x173 || reg == 0x174 || reg == 0x175 ||
        reg == 0x176 || reg == 0x179 || reg == 0x17a || reg == 0x185;
}
static uint16_t reg_read(unsigned reg) {
 assert(reg < 0x2000);
 if (mode == 1 && reg == 0) return 0;
 if(reg==0x5f7)return mode==50?0:1;
 if(mode==41 && reg==0x1714) {
  if(bits==48) gpio_reads++;
  return (regs[reg]&0x7fff)|((gpio_reads&1)?0x8000:0);
 }
 if (reg == 0x1881) return (regs[0x171] & 1) && active_n == 1000 && mode != 20 ? 0x100 : 0;
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
    if (writes != ignore_write && !(mode==51 && reg==0x700 && (regs[reg]&255)==0x38 && !(value&255))) {
     if (reg == 0x171 && mode == 25 && enabled_once) value |= 1; /* stuck enabled */
     if (reg == 0x171 && (value & 1)) {
      enabled_once = 1;
      assert(regs[0x173] == 0 && regs[0x174] == 0);
      assert((regs[0x175] & 0xf00) == 0x200 && (regs[0x176] & 0xcf) == 1);
      assert((regs[0x179] & 0x3c) == 8 && (regs[0x17a] & 0xf800) == 0x2800);
      assert((regs[0x172] & 0x3ff) == 1000);
      assert((regs[0x100] & 0x43) == 0x41);
     }
     if(((reg==0x1710||reg==0x1714)&&!(value&0x1ff)) || (reg==0x500&&(value&0x40)) || (reg==0x101&&(value&0x40)))
      assert(amp_quiet());
     if (reg == 0x172 && (value & 0x8000)) active_n = value & 0x3ff;
     /* Hardware sets read-only x_STSn when a source is selected. */
     if(reg>=0x700 && reg<=0x70e && !(reg&1))value=(value&0x7fff)|((value&255)&&mode!=52?0x8000:0);
     regs[reg] = reg == 0x172 && mode != 26 ? value & 0x7fff : value;
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
 assert(core == 0x0200f000 && (ppid == 0x145 || ppid == 0x11d || ppid == 0xc5)); return ppid;
}
static bool ri_spmi_read(vm_offset_t obs, unsigned reg, unsigned n, uint32_t *v) {
 assert(n == 1 && (obs == 0x145 || obs == 0x11d || obs == 0xc5));
 if (obs == 0xc5) {
  pmic_reads++;
  if (mode == 30 || (mode == 32 && pmic_writes)) return false;
  if (mode == 31 && pmic_reads > 7) { *v = 0xffff; return true; }
  switch (reg) { case 4:*v=0x10;break; case 5:*v=1;break; case 0x40:*v=pmic_mode;break;
   case 0x42:*v=pmic_pull;break;
   case 0x41:case 0x44:*v=0;break; case 0x46:*v=0x80;break; default:assert(0); }
  return true;
 }
 switch (reg) {
 case 4: *v = obs == 0x145 ? 0x21 : 3; break;
 case 5: *v = obs == 0x145 ? 0x2a : 0xa; break;
 case 8: case 0x46: *v = 0x80; break;
 default: assert(0);
 }
 return true;
}
static vm_offset_t ri_spmi_chnl(vm_offset_t core, unsigned ppid) {
 assert(core == 0x0200f000 && ppid == 0xc5); return cmd_missing ? 0 : ppid;
}
static bool ri_spmi_write(vm_offset_t channel, unsigned reg, uint8_t value) {
 assert(channel == 0xc5 && (reg == 0x40 || reg == 0x42));
 pmic_writes++;
 if (pmic_writes == pmic_ignore || (mode == 33 && pmic_writes > 2)) return false;
 if (reg == 0x40) { assert(value == 0 || value == 0x1c); pmic_mode = value; }
 else { assert(value == 4 || value == 5); pmic_pull = value; }
 return true;
}
#include "codec-probe-added.h"
#include "codec-clock-added.h"
static int ri_speaker_lock;
static unsigned codec_held,speaker_held;
static void lck_mtx_lock(int *p) {if(p==&ri_codec_lock){assert(!codec_held && !speaker_held);codec_held=1;}else {assert(p==&ri_speaker_lock && codec_held && !speaker_held);speaker_held=1;}}
static void lck_mtx_unlock(int *p) {if(p==&ri_speaker_lock){assert(speaker_held);speaker_held=0;}else {assert(p==&ri_codec_lock && codec_held && !speaker_held);codec_held=0;}}
static bool ri_afe_mute_hold;
static struct {bool complete;} ri_stream;
static struct {bool muted,asm_open,adm_open,afe_started;int cleanup_error;} ri_route;
static struct {bool poisoned;} ri_pcm;
static struct ri_stream_state {bool complete,mapped,in_flight,afe_prepared,tone,native_pcm,queued_pcm;unsigned submitted;char note[1536];} ri_live_stream;
static struct {bool enabled,stopped,cutoff;int error;uint64_t armed_at,unmuted_at,muted_at,max_check_ns;} ri_tone;
static int (*ri_stream_output_start)(void),(*ri_stream_output_check)(void);
static void (*ri_stream_output_stop)(void);
static int ri_tone_start(void){ri_tone.enabled=true;return 0;}
static int ri_tone_check(void){return 0;}
static void ri_tone_stop(void){ri_tone.stopped=true;}
static unsigned live_calls,prepare_calls;
static bool native_ready;static unsigned native_claims;
static bool ri_native_pcm_claim(void){assert(!codec_held && !speaker_held);if(!native_ready || native_claims)return false;native_claims++;return true;}
static int ri_stream_prepare_live(void){assert(!codec_held && !speaker_held);prepare_calls++;if(mode==55)return EREMOTE;ri_live_stream.afe_prepared=ri_route.afe_started=true;return 0;}
static void ri_stream_cleanup(struct ri_stream_state *state){assert(!speaker_held);if(!ri_pcm.poisoned)state->afe_prepared=ri_route.afe_started=false;}

static int ri_stream_execute(struct ri_stream_state *state,unsigned session) {
 assert(state==&ri_live_stream && session==5 && codec_held && !speaker_held);
 assert((regs[0x700]&0x80ff)==0x8038 && (regs[0x708]&0x80ff)==0x8038 && regs[0x519]==3 && (regs[0x5f5]&1));
 assert(amp_quiet() && amp[0x4800]==0x10000 && amp[0x4820]==0xa5000100);live_calls++;
 assert(state->afe_prepared && ri_route.afe_started);
 if(state->tone){assert(ri_stream_output_start && ri_stream_output_check && ri_stream_output_stop);assert(!ri_stream_output_start() && !ri_stream_output_check());ri_stream_output_stop();}
 state->submitted=4;state->complete=mode!=53 && mode!=54;
 if(mode==53){state->mapped=state->in_flight=ri_pcm.poisoned=ri_route.afe_started=true;return ETIMEDOUT;}
 state->afe_prepared=ri_route.afe_started=false;
 if(mode==54)return EREMOTE;
 return 0;
}
static struct {bool address_valid,enabled,uncertain;} ri_ngd;
static struct {bool attempted,complete,connected,defined,active,uncertain;} ri_channel;
static int (*ri_channel_active_test)(void);
static bool ri_route_ready(void){return true;}
static int ri_channel_test(void) {
 if(ri_channel.attempted)return EALREADY;ri_channel.attempted=true;
 ri_channel.connected=ri_channel.defined=ri_channel.active=true;
 int error=ri_channel_active_test?ri_channel_active_test():0;
 if(!ri_channel.uncertain)ri_channel.connected=ri_channel.defined=ri_channel.active=false;
 ri_channel.complete=!error;return error;
}
static int quiet_printf(const char *format,...) {(void)format;return 0;}
#define printf quiet_printf
static int (*ri_pipeline_health)(void);
static int ri_live_health(void){return 0;}
#include "codec-route-added.h"
#undef printf
static void reset(unsigned scenario) {
 mode = scenario; writes = ignore_write = bits = reply = elapsed = enabled_once = 0;
 ri_clock_attempted = ri_clock_surveyed = 0; active_n = 8;
 pmic_mode = 0; pmic_pull = 4; pmic_writes = pmic_ignore = cmd_missing = pmic_reads = 0; ri_clock_mclk_cmd = 0;
 for (unsigned i = 0; i < 4; i++) { cfg[i] = 1; out[i] = 0; }
 memset(amp,0,sizeof(amp));amp[0]=0x35a40; amp[0x2018]=0x21; amp[0x2024]=0x100;
 amp[0x4808]=0x20200233;amp[0x2c04]=0x833;amp[0x4800]=0x10001;amp[0x4840]=24;amp[0x4830]=24;amp[0x4820]=0xa5000103;
 memcpy(amp_initial,amp,sizeof(amp));amp_writes=amp_ignore=amp_no_lock=gpio_reads=0;
 memset(regs, 0, sizeof(regs)); regs[0] = 0x6360; regs[1] = 1;
 regs[0x1710]=0x2001;regs[0x1714]=0xa001;
 regs[0x1711]=regs[0x1715]=0xf000; regs[0x100] = 2; regs[0x101] = 0x404; regs[0x171] = 2; regs[0x172] = 8;
 regs[0x173] = 24; regs[0x174] = 125; regs[0x185] = 1;
 regs[0x1841] = regs[0x1941] = 0xffff;
 if (scenario == 21) regs[0x101] |= 0x40;
 if (scenario == 22) regs[0x171] |= 1;
 if (scenario == 23) regs[0x1841] &= ~0x100;
 if (scenario == 28) regs[0x100] = 0x42;
 if (scenario == 29) regs[0x1941] &= ~0x100;
 memcpy(initial, regs, sizeof(regs));
}
static void check_pins(void) { for (unsigned i = 0; i < 4; i++) assert(cfg[i] == 1 && out[i] == 0); }
static void check_restore(void) {
 check_pins(); assert(!memcmp(regs, initial, sizeof(regs)));
 assert(strstr(ri_clock_note, "clock_restored=1")); assert(active_n == 8);
}
int clock_baseline_main(void) {
 reset(0);amp_ignore=1;
 assert(ri_clock_run_impl(true,true,true)==EIO);
 assert(strstr(ri_clock_note,"amp_error=write_failed amp_reg=0x2024"));
 assert(ri_i2s_amp_restored&&ri_i2s_codec_restored);check_restore();
 reset(41);assert(!ri_clock_run_impl(false,false,true));
 assert(!writes&&!amp_writes&&!pmic_writes);check_pins();
 reset(41);assert(!ri_clock_run_impl(true,true,true));check_restore();
 assert(ri_i2s_amp_locked&&ri_i2s_amp_restored&&ri_i2s_codec_restored);
 for(unsigned fn=2;fn<=4;fn++) {
  reset(0);regs[0x1710]=(regs[0x1710]&~0x1ffu)|fn;
  assert(ri_clock_run_impl(true,true,true)==EBUSY);
  assert(!writes&&!amp_writes&&!pmic_writes);check_pins();
 }
 reset(0);regs[0x1711]&=~0x8000u;
 assert(ri_clock_run_impl(true,true,true)==EBUSY);
 assert(!writes&&!amp_writes&&!pmic_writes);check_pins();
 reset(0);amp[0]=0;
 assert(ri_clock_run_impl(false,false,true)==EIO);
 assert(strstr(ri_clock_note,"i2s_snapshot_error=amp_id"));
 assert(strstr(ri_clock_note,"snapshot_want=0x35a40"));
 assert(!writes&&!amp_writes&&!pmic_writes);check_pins();
 reset(0); assert(!ri_clock_run(false)); assert(!writes); check_pins();
 assert(strstr(ri_clock_note, "snapshot=1")); assert(!ri_clock_run(false)); assert(!writes);
 reset(0); assert(!ri_clock_run(true)); check_restore(); assert(strstr(ri_clock_note, "locked=1"));
 unsigned old_writes = writes; assert(ri_clock_run(true) == EALREADY); assert(writes == old_writes);
 for (unsigned n = 1; n <= 13; n++) {
  reset(0); ignore_write = n;
  int rc = ri_clock_run(true);
  assert(rc == 0 || rc == EIO || rc == ETIMEDOUT); /* N strobe failures may be detected only by lack of lock. */
  check_restore();
 }
 reset(26); assert(!ri_clock_run(true)); check_restore();
 reset(20); assert(ri_clock_run(true) == ETIMEDOUT); check_restore();
 assert(strstr(ri_clock_note, "locked=0"));
 reset(0); ignore_write = 15; assert(ri_clock_run(true) == EIO); check_pins();
 assert(strstr(ri_clock_note, "restore_reg=0x173"));
 unsigned scenarios[] = {1, 21, 22, 23, 28, 29};
 for (unsigned i = 0; i < sizeof(scenarios)/sizeof(scenarios[0]); i++) {
  reset(scenarios[i]); assert(ri_clock_run(true)); assert(!writes); check_pins();
 }
 reset(25); assert(ri_clock_run(true) == EIO); check_pins();
 assert(strstr(ri_clock_note, "clock_restored=0"));
 assert(!ri_clock_write_allowed(0x200) && !ri_clock_write_allowed(0x400));
 reset(0); assert(!ri_clock_run_impl(true,true,false)); check_restore();
 assert(pmic_mode == 0 && pmic_pull == 4 && pmic_writes == 4);
 assert(strstr(ri_clock_note,"route_restored=1"));
 reset(20); assert(ri_clock_run_impl(true,true,false) == ETIMEDOUT); check_restore();
 assert(pmic_mode == 0 && pmic_pull == 4);
 for (unsigned fail = 1; fail <= 4; fail++) {
  reset(0); pmic_ignore = fail;
  int rc = ri_clock_run_impl(true,true,false); assert(rc == 0 || rc == EIO);
  check_restore(); assert(pmic_mode == 0 && pmic_pull == 4);
 }
 reset(0); cmd_missing = 1; assert(ri_clock_run_impl(true,true,false) == ENXIO);
 assert(!pmic_writes && !writes); check_pins();
 reset(0); pmic_mode = 0x1c; assert(ri_clock_run_impl(true,true,false) == EBUSY);
 assert(!pmic_writes && !writes); check_pins();
 reset(0); pmic_pull = 5; assert(ri_clock_run_impl(true,true,false) == EBUSY);
 assert(!pmic_writes && !writes); check_pins();
 for (unsigned scenario = 30; scenario <= 31; scenario++) {
  reset(scenario); assert(ri_clock_run_impl(true,true,false) == EBUSY);
  assert(!pmic_writes && !writes); check_pins();
 }
 for (unsigned scenario = 32; scenario <= 33; scenario++) {
  reset(scenario); assert(ri_clock_run_impl(true,true,false) == EIO); check_pins();
  assert(strstr(ri_clock_note,"route_restored=0")); assert(pmic_writes <= 8);
 }
 reset(0); assert(!ri_clock_run_impl(false,false,true));assert(!amp_writes&&!writes);check_pins();
 reset(0); assert(!ri_clock_run_impl(true,true,true));check_restore();
 assert(ri_i2s_amp_locked&&ri_i2s_amp_restored&&ri_i2s_codec_restored);
 assert(!memcmp(amp,amp_initial,sizeof(amp)));
 unsigned success_amp_writes=amp_writes;
 reset(0);amp_no_lock=1;assert(ri_clock_run_impl(true,true,true)==ETIMEDOUT);check_restore();
 assert(!memcmp(amp,amp_initial,sizeof(amp)));
 for(unsigned fail=1;fail<=success_amp_writes;fail++) {
  reset(0);amp_ignore=fail;int rc=ri_clock_run_impl(true,true,true);assert(rc!=0);check_pins();
  assert(ri_i2s_codec_restored);
  if(!ri_i2s_amp_restored) assert(amp_quiet());
  else assert(!memcmp(amp,amp_initial,sizeof(amp)));
 }
 reset(0);assert(!ri_clock_run_impl(true,true,true));unsigned success_codec_writes=writes;
 for(unsigned fail=1;fail<=success_codec_writes;fail++) {
  reset(0);ignore_write=fail;int rc=ri_clock_run_impl(true,true,true);check_pins();
  if(!ri_i2s_codec_restored||!ri_i2s_amp_restored) {assert(rc==EIO);assert(amp_quiet());}
 }
 reset(0);regs[0x519]=1;assert(ri_clock_run_impl(true,true,true)!=0);assert(!amp_writes);check_pins();
 puts("PASS: muted I2S ordering, Class-D disable, slave/master exclusion, no PCM data, amp lock/timeout and failed-write cleanup");
 puts("PASS: PMIC route ownership, fail-closed channel lookup, write/read failures, timeout and full route restoration");
 puts("PASS: FLL framing, full 32k recipe, latched/self-clearing update strobes, repeated snapshots, rollback and stuck-enable detection");
 return 0;
}

static void path_reset(unsigned scenario)
{
 reset(scenario);native_ready=true;native_claims=0;memset(&ri_tone,0,sizeof(ri_tone));ri_stream_output_start=NULL;ri_stream_output_check=NULL;ri_stream_output_stop=NULL;live_calls=prepare_calls=0;memset(&ri_live_stream,0,sizeof(ri_live_stream));memset(&ri_pcm,0,sizeof(ri_pcm));memset(&ri_route,0,sizeof(ri_route));memset(&ri_codec_path,0,sizeof(ri_codec_path));memset(&ri_channel,0,sizeof(ri_channel));
 ri_clock_data_test=NULL;ri_channel_active_test=NULL;ri_clock_data_restored=true;
 codec_held=speaker_held=0;ri_afe_mute_hold=ri_route.muted=ri_stream.complete=ri_ngd.address_valid=ri_ngd.enabled=true;ri_ngd.uncertain=false;
 amp[0x2024]|=0x10;memcpy(amp_initial,amp,sizeof(amp));
 regs[0x1712]=0x2001;regs[0x1713]=0xf000;regs[0x505]=64;regs[0x509]=4;regs[0x50a]=5;
 for(unsigned r=0x700;r<0x710;r++)regs[r]=(r&1)?0x80:0;
 memcpy(initial,regs,sizeof(regs));
}
int main(void)
{
 clock_baseline_main();
 path_reset(0);assert(!ri_codec_path_run());assert(ri_codec_path.complete && ri_codec_path.seen && ri_codec_path.restored && ri_i2s_amp_locked);
 check_restore();assert(!memcmp(amp,amp_initial,sizeof(amp)) && !codec_held && !speaker_held && ri_channel.complete);
 assert(ri_codec_path_run()==EALREADY);
 unsigned total_writes=writes;
 path_reset(60);assert(ri_codec_path_run()==EIO && !ri_codec_path.complete && ri_codec_path.restored);check_restore();assert(!memcmp(amp,amp_initial,sizeof(amp)) && amp[0x4820]==0xa5000103);
 path_reset(52);assert(ri_codec_path_run()==ETIMEDOUT && !ri_codec_path.complete && ri_codec_path.restored);check_restore();
 path_reset(50);assert(ri_codec_path_run()==ENXIO && !ri_codec_path.complete && ri_codec_path.restored);check_restore();
 path_reset(0);regs[0x5e5]=8;memcpy(initial,regs,sizeof(regs));assert(ri_codec_path_run()==EBUSY && !ri_codec_path.touched && !writes && !amp_writes);check_pins();assert(!memcmp(regs,initial,sizeof(regs)));
 path_reset(0);regs[0x5d2]=1;memcpy(initial,regs,sizeof(regs));assert(ri_codec_path_run()==EBUSY && !writes && !amp_writes);check_pins();assert(!memcmp(regs,initial,sizeof(regs)));
 path_reset(0);regs[0x700]=0x60;memcpy(initial,regs,sizeof(regs));assert(ri_codec_path_run()==EBUSY && !writes && !amp_writes);check_pins();assert(!memcmp(regs,initial,sizeof(regs)));
 path_reset(0);regs[0x1712]=2;memcpy(initial,regs,sizeof(regs));assert(ri_codec_path_run()==EBUSY && !writes && !amp_writes);check_pins();assert(!memcmp(regs,initial,sizeof(regs)));
 path_reset(0);ri_stream.complete=false;assert(ri_codec_path_run()==EBUSY && !writes && !amp_writes);
 for(unsigned index=1;index<=total_writes;index++){
  path_reset(0);ignore_write=index;int error=ri_codec_path_run();assert(error==0 || error==EIO || error==ETIMEDOUT);
  assert(!codec_held && !speaker_held && (amp[0x2024]&0x10));
  if(ri_codec_path.restored && ri_i2s_codec_restored && ri_i2s_amp_restored && strstr(ri_clock_note,"clock_restored=1"))assert(!memcmp(regs,initial,sizeof(regs)) && !memcmp(amp,amp_initial,sizeof(amp)));
 }
 path_reset(51);assert(ri_codec_path_run()==EIO && !ri_codec_path.restored && !ri_i2s_amp_restored && !ri_i2s_codec_restored);
 assert(amp_quiet() && !amp[0x4800] && !(regs[0x101]&0x40) && !regs[0x519] && (regs[0x102]&31)==3);
 check_pins();assert(!codec_held && !speaker_held);
 path_reset(0);assert(!ri_codec_path_run_mode(true) && live_calls==1 && ri_codec_path.complete && ri_live_stream.complete);check_restore();assert(!speaker_held && !codec_held);
 path_reset(0);assert(!ri_codec_path_run_mode(4) && ri_live_stream.queued_pcm && ri_live_stream.tone && live_calls==1);check_restore();
 path_reset(0);assert(!ri_codec_path_run_mode(3) && native_claims==1 && ri_live_stream.native_pcm && ri_live_stream.tone && live_calls==1);check_restore();
 path_reset(0);native_ready=false;assert(ri_codec_path_run_mode(3)==EBUSY && !ri_codec_path.attempted && !writes && !amp_writes && !live_calls);
 path_reset(53);assert(ri_codec_path_run_mode(true)==ETIMEDOUT && live_calls==1 && ri_channel.uncertain && ri_channel.active && ri_live_stream.mapped && ri_codec_path.restored);check_restore();assert((amp[0x2024]&0x10) && !speaker_held && !codec_held);
 path_reset(55);assert(ri_codec_path_run_mode(true)==EREMOTE && prepare_calls==1 && !live_calls && !writes && !amp_writes && !ri_channel.attempted);
 path_reset(54);assert(ri_codec_path_run_mode(true)==EREMOTE && live_calls==1 && !ri_channel.uncertain && !ri_channel.active && ri_codec_path.restored);check_restore();
 path_reset(0);assert(!ri_codec_path_run_mode(2) && ri_codec_path.complete && ri_tone.enabled && ri_tone.stopped);check_restore();assert(!ri_stream_output_start && !ri_stream_output_stop);
 assert(!ri_clock_write_allowed(0x5f7) && ri_clock_read_allowed(0x5f7) && !ri_clock_write_allowed(0x5d2));
 assert(!ri_clock_write_allowed(0x400) && !ri_clock_write_allowed(0x5f6));
 puts("actual-code muted SLIMRX/AIF route, FLL/AMP clock locks, GPIO SPI framing and rollback tests passed");
 return 0;
}
