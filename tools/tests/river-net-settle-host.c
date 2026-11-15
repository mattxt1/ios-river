#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include "river-net-settle.h"
int main(void){struct river_net_settle s;river_net_settle_reset(&s,100);assert(!river_net_settle_due(&s,101,1) && !river_net_settle_due(&s,102,0));for(int i=0;i<8;i++){time_t now=102+i*2;assert(river_net_settle_due(&s,now,1));assert(river_net_settle_observed(&s,now,0)==(i<3));}assert(s.republishes==3 && !river_net_settle_due(&s,1000,1));river_net_settle_reset(&s,200);for(int i=0;i<8;i++)assert(!river_net_settle_observed(&s,202+i*2,-1));assert(!s.republishes);river_net_settle_reset(&s,300);assert(!river_net_settle_observed(&s,302,1));assert(river_net_settle_observed(&s,304,0)==1);assert(!river_net_settle_observed(&s,306,1) && s.republishes==1);puts("bounded service-state reconciliation tests passed");return 0;}
