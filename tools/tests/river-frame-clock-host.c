#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "river_frame_clock.h"
int main(void){
 struct river_frame_clock c={.period=16666667};uint64_t now=1000000000,first=river_frame_clock_arm(&c,now);
 assert(first==now+c.period);
 for(unsigned i=0;i<600;i++){
  now=c.due;river_frame_clock_fire(&c,now);
  uint64_t previous=c.due;now+=3800000;assert(river_frame_clock_arm(&c,now)==previous+c.period);
 }
 assert(c.callbacks==600 && !c.skipped && !c.max_late && c.due==first+600*c.period);
 uint64_t prior=c.due;now=prior+3*c.period+1;river_frame_clock_fire(&c,now);
 assert(river_frame_clock_arm(&c,now)==prior+4*c.period && c.skipped==3 && c.max_late==3*c.period+1);
 prior=c.due;river_frame_clock_fire(&c,prior);assert(river_frame_clock_arm(&c,prior+c.period)==prior+2*c.period && c.skipped==4);
 /* A long suspend does not produce a burst of obsolete callbacks. */
 now=c.due+3600000000000ULL;river_frame_clock_fire(&c,now);uint64_t next=river_frame_clock_arm(&c,now);assert(next>now && next-now<=c.period);
 struct river_frame_clock disabled={0};assert(!river_frame_clock_arm(&disabled,123));
 puts("PASS actual River frame clock:600frames retain16.667ms cadence despite3.8ms presentation; late frames and long pauses skip old deadlines without catch-up bursts; late counters and disabled guard");
}
