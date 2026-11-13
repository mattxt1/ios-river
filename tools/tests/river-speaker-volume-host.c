#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static bool ri_speaker_ok;
static uint32_t ri_cs35l41_dig_vol, ri_cs35l41_out_mute;
static uint32_t control, digital;
static unsigned reads, writes, fail_read, fail_write, volume_writes;
static bool corrupt_volume;
static bool ri_cs35l41_rd(uint32_t reg, uint32_t *value)
{
 if (++reads == fail_read) return false;
 assert(reg == 0x2024 || reg == 0x6000);
 *value = reg == 0x2024 ? control : digital;
 if (reg == 0x6000 && corrupt_volume && volume_writes) *value ^= 8;
 return true;
}
static bool ri_cs35l41_wr(uint32_t reg, uint32_t value)
{
 if (++writes == fail_write) return false;
 assert(reg == 0x2024 || reg == 0x6000);
 if (reg == 0x2024) control = value;
 else { digital = value; volume_writes++; }
 return true;
}
#include "volume-actual.h"

static void reset(void)
{
 ri_speaker_ok = true;
 control = 0xa5000015;
 digital = 0xb5000005;
 ri_cs35l41_dig_vol = ri_cs35l41_out_mute = 0;
 reads = writes = fail_read = fail_write = volume_writes = 0;
 corrupt_volume = false;
}
int main(void)
{
#ifdef EXPECT_BASELINE
 reset(); river_speaker_set_volume_locked(80, false);
 assert(digital == 80); /* Old implementation does not encode -10dB. */
 assert((digital & 0x3ff8) != 0x3d80);
 reset(); fail_write = 1; river_speaker_set_volume_locked(80, false);
 assert(!(control & 0x10)); /* Old implementation unmutes after write failure. */
 puts("baseline reproduces wrong attenuation and failed-write unmute");
#else
 const uint32_t attenuation[] = {0, 1, 80, 720, UINT32_MAX};
 const uint32_t expected[] = {0, 0x3ff8, 0x3d80, 0x2980, 0x2980};
 for (unsigned i = 0; i < sizeof(attenuation)/sizeof(attenuation[0]); i++) {
  reset(); river_speaker_set_volume_locked(attenuation[i], false);
  assert((digital & 0x3ff8) == expected[i]);
  assert((digital & ~0x3ff8u) == (0xb5000005u & ~0x3ff8u));
  assert(control == 0xa5000005);
  assert(ri_cs35l41_dig_vol == digital && ri_cs35l41_out_mute == control);
 }
 reset(); river_speaker_set_volume_locked(80, true);
 assert((digital & 0x3ff8) == 0x2678 && (control & 0x10));
 for (unsigned i = 1; i <= 5; i++) {
  reset(); fail_read = i; river_speaker_set_volume_locked(80, false);
  assert(control & 0x10);
 }
 for (unsigned i = 1; i <= 3; i++) {
  reset(); fail_write = i; river_speaker_set_volume_locked(80, false);
  assert(control & 0x10);
 }
 reset(); corrupt_volume = true; river_speaker_set_volume_locked(80, false);
 assert(control & 0x10);
 reset(); ri_speaker_ok = false; river_speaker_set_volume_locked(80, false);
 assert(reads == 0 && writes == 0);
 puts("signed attenuation, mute, field preservation and failure guards: PASS");
#endif
 return 0;
}
