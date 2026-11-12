#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
static bool ri_speaker_ok=true,read_failure;
static int ri_speaker_lock;
static uint32_t control,volume,ri_cs35l41_dig_vol,ri_cs35l41_out_mute;
static unsigned writes;
static void lck_mtx_lock(int *p) { assert(!*p);*p=1; }
static void lck_mtx_unlock(int *p) { assert(*p);*p=0; }
static bool ri_cs35l41_rd(uint32_t reg,uint32_t *value) {
 assert(ri_speaker_lock);if(read_failure)return false;
 assert(reg==0x2024||reg==0x6000);*value=reg==0x2024?control:volume;return true;
}
static bool ri_cs35l41_wr(uint32_t reg,uint32_t value) {
 assert(ri_speaker_lock);assert(reg==0x2024||reg==0x6000);writes++;
 if(reg==0x2024)control=value;else volume=value;return true;
}
#include "speaker-volume-added.h"
int main(void) {
 control=0x101;
 river_speaker_set_volume(40,true);assert(control==0x111&&volume==0x4cf);
 assert(ri_cs35l41_out_mute==0x111&&ri_cs35l41_dig_vol==0x4cf);
 river_speaker_set_volume(40,false);assert(control==0x101&&volume==40);
 river_speaker_set_volume(UINT32_MAX,false);assert(volume==0x400);
 unsigned before=writes;read_failure=true;river_speaker_set_volume(0,true);assert(writes==before);
 read_failure=false;ri_speaker_ok=false;river_speaker_set_volume(0,false);assert(writes==before);
 assert(!ri_speaker_lock);
 puts("PASS: correct bit4 mute, unrelated-bit preservation, volume clamp and serialized read/write sequence");
}
