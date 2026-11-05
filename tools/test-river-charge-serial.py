from pathlib import Path
import subprocess, sys, tempfile
if len(sys.argv)!=2: raise SystemExit('usage: test-river-charge-serial.py PATCHED_SWEET_KEYS_C')
s=Path(sys.argv[1]).read_text(); a=s.index('static int\nriver_charge_serial_sysctl'); b=s.index('\nSYSCTL_PROC(_debug, OID_AUTO, river_charge_serial',a); fn=s[a:b]
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
static int ri_power_ready,ri_power_busy,ri_pon_ok,ri_pon_survey_gate,mode,calls,output_error; static uint32_t ri_pon_deferred; static vm_offset_t ri_charge_chgr=1;
static bool ri_charge_alt_read(vm_offset_t p,unsigned a,uint32_t *s,uint32_t *v){assert(p==1&&a==4&&ri_pon_survey_gate==1);calls++;*s=mode==1?9:1;*v=2;return mode!=1;}
static void delay(unsigned n){assert(ri_pon_survey_gate==1); if(n==10)ri_pon_deferred++;}
static uint32_t ri_rd(vm_offset_t p,unsigned r){assert(p==1&&r==0x18&&ri_pon_survey_gate==1);return mode==2?0:2;}
static int sysctl_handle_string(void *o,char *s,int l,struct sysctl_req *r){(void)o;(void)l;(void)r;assert(!ri_pon_survey_gate || mode==3);
 if(mode==0)assert(strstr(s,"two=64")&&strstr(s,"changed10=0 changed100=0 pon_deferred=64"));
 if(mode==1)assert(strstr(s,"errors=64 last_error=0x9"));
 if(mode==2)assert(strstr(s,"changed10=64 changed100=64"));
 if(mode==3)assert(strstr(s,"gate_busy=64"));
 return output_error?EIO:0;}
'''
main=r'''
int main(void){struct sysctl_req r={0};assert(river_charge_serial_sysctl(0,0,0,&r)==ENXIO);ri_power_ready=ri_pon_ok=1;ri_power_busy=1;assert(river_charge_serial_sysctl(0,0,0,&r)==EBUSY);ri_power_busy=0;for(mode=0;mode<4;mode++){calls=0;ri_pon_survey_gate=mode==3;assert(!river_charge_serial_sysctl(0,0,0,&r));assert(calls==(mode==3?0:64));assert(ri_pon_survey_gate==(mode==3));assert(!ri_power_busy);}mode=0;ri_pon_survey_gate=0;output_error=1;assert(river_charge_serial_sysctl(0,0,0,&r)==EIO&&!ri_power_busy&&!ri_pon_survey_gate);puts("PASS: exclusion, data changes, error release, busy, unavailable and output failure");}
'''
with tempfile.TemporaryDirectory(prefix='river-serial-') as d:
 src=Path(d)/'test.c'; exe=Path(d)/'test';src.write_text(pre+fn+main);subprocess.run(['clang','-fsanitize=address,undefined',str(src),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
