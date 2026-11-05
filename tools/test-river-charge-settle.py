from pathlib import Path
import subprocess, sys, tempfile
if len(sys.argv)!=2: raise SystemExit('usage: test-river-charge-settle.py PATCHED_SWEET_KEYS_C')
s=Path(sys.argv[1]).read_text();a=s.index('static int\nriver_charge_settle_sysctl');b=s.index('\nSYSCTL_PROC(_debug, OID_AUTO, river_charge_settle',a);fn=s[a:b]
pre=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
typedef uintptr_t vm_offset_t;
struct sysctl_req { int unused; };
#define SYSCTL_HANDLER_ARGS (void *oidp, void *arg1, int arg2, struct sysctl_req *req)
static int ri_power_ready,ri_power_busy,mode,waited,calls,output_error;
static vm_offset_t ri_charge_chgr=1;
static bool ri_charge_alt_read(vm_offset_t p,unsigned addr,uint32_t *s,uint32_t *v){assert(p==1&&addr==4);calls++;waited=0;*s=mode==3?3:1;*v=(mode==1||mode==2)?0:2;return mode!=3;}
static void delay(unsigned n){waited+=n;}
static uint32_t ri_rd(vm_offset_t p,unsigned reg){assert(p==1&&reg==0x18);if(mode==1)return 2;if(mode==2)return waited>=100?2:0;if(mode==4)return 0;return 2;}
static int sysctl_handle_string(void *o,char *s,int l,struct sysctl_req *r){(void)o;(void)l;(void)r;
 if(mode==0){assert(strstr(s,"first_two=64"));assert(strstr(s,"changed10=0 changed100=0"));}
 if(mode==1){assert(strstr(s,"first_zero=64"));assert(strstr(s,"zero_to_two10=64 zero_to_two100=64"));}
 if(mode==2){assert(strstr(s,"changed10=0 changed100=64"));assert(strstr(s,"zero_to_two10=0 zero_to_two100=64"));}
 if(mode==3){assert(strstr(s,"errors=64 last_error=0x3"));assert(strstr(s,"changed10=0"));}
 if(mode==4){assert(strstr(s,"two_to_zero=64"));}
 return output_error?EIO:0;
}
'''
main=r'''
int main(void){struct sysctl_req r={0};assert(river_charge_settle_sysctl(0,0,0,&r)==ENXIO&&calls==0);ri_power_ready=1;ri_power_busy=1;assert(river_charge_settle_sysctl(0,0,0,&r)==EBUSY&&calls==0);ri_power_busy=0;for(mode=0;mode<5;mode++){calls=0;assert(river_charge_settle_sysctl(0,0,0,&r)==0&&calls==64&&!ri_power_busy);}mode=0;output_error=1;assert(river_charge_settle_sysctl(0,0,0,&r)==EIO&&!ri_power_busy);puts("PASS: stable, post-DONE transitions, errors, unavailable and busy");}
'''
with tempfile.TemporaryDirectory(prefix='river-settle-') as d:
 src=Path(d)/'test.c';exe=Path(d)/'test';src.write_text(pre+fn+main);subprocess.run(['clang','-fsanitize=address,undefined',str(src),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
