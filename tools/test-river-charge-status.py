from pathlib import Path
import subprocess, sys, tempfile
if len(sys.argv)!=2: raise SystemExit("usage: test-river-charge-status.py PATCHED_SWEET_KEYS_C")
s=Path(sys.argv[1]).read_text(); a=s.index("static int\nriver_charge_status_sysctl"); b=s.index("\nSYSCTL_PROC(_debug, OID_AUTO, river_charge_status",a); fn=s[a:b]
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
static bool ri_charge_alt_read(vm_offset_t p,unsigned addr,uint32_t *s,uint32_t *v){assert(p==1&&addr==4);calls++;waited=0;*s=mode==3?9:1;*v=2;return mode!=3;}
static void delay(unsigned n){waited+=n;}
static uint32_t ri_rd(vm_offset_t p,unsigned reg){assert(p==1);if(reg==8)return mode==2&&waited>=100?9:1;assert(reg==0x18);return mode==0?2:0;}
static int sysctl_handle_string(void *o,char *s,int l,struct sysctl_req *r){(void)o;(void)l;(void)r;
 if(mode==0)assert(strstr(s,"changed10=0 changed100=0 status10=0 status100=0"));
 if(mode==1)assert(strstr(s,"changed10=64 changed100=64 status10=0 status100=0 change_status10=0 change_status100=0"));
 if(mode==2)assert(strstr(s,"status10=0 status100=64 change_status10=0 change_status100=64 lateerr10=0 lateerr100=64"));
 if(mode==3)assert(strstr(s,"errors=64 last_error=0x9"));
 return output_error?EIO:0;
}
'''
main=r'''
int main(void){struct sysctl_req r={0};assert(river_charge_status_sysctl(0,0,0,&r)==ENXIO);ri_power_ready=1;ri_power_busy=1;assert(river_charge_status_sysctl(0,0,0,&r)==EBUSY);ri_power_busy=0;for(mode=0;mode<4;mode++){calls=0;assert(!river_charge_status_sysctl(0,0,0,&r)&&calls==64&&!ri_power_busy);}mode=0;output_error=1;assert(river_charge_status_sysctl(0,0,0,&r)==EIO&&!ri_power_busy);puts("PASS: unchanged and changed status/data, errors, busy and output failure");}
'''
with tempfile.TemporaryDirectory(prefix="river-status-") as d:
 src=Path(d)/"test.c"; exe=Path(d)/"test"; src.write_text(pre+fn+main); subprocess.run(["clang","-fsanitize=address,undefined",str(src),"-o",str(exe)],check=True); subprocess.run([str(exe)],check=True)
