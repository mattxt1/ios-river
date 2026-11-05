from pathlib import Path
import subprocess, sys, tempfile
if len(sys.argv)!=2: raise SystemExit('usage: test-river-charge-alt.py PATCHED_SWEET_KEYS_C')
s=Path(sys.argv[1]).read_text(); a=s.index('static bool\nri_charge_alt_read'); b=s.index('\nSYSCTL_PROC(_debug, OID_AUTO, river_charge_alt',a); fn=s[a:b]
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
static int ri_power_ready,ri_power_busy,mode,phase,writes,output_error;
static vm_offset_t ri_charge_chgr=1;
static void ri_wr(vm_offset_t p,unsigned reg,uint32_t v) {
 assert(p==1 && reg==0); assert(v==((1u<<27)|(4u<<4)) || v==((1u<<27)|(5u<<4)));
 phase=((v>>4)&255)==5; writes++;
}
static uint32_t ri_rd(vm_offset_t p,unsigned reg) {
 assert(p==1);
 if(reg==8) return mode==3?3:1;
 assert(reg==0x18);
 if(mode==1) return 2;
 if(mode==2) return 0;
 return phase?0x80:2;
}
static void delay(unsigned n){(void)n;}
static int sysctl_handle_string(void *o,char *s,int l,struct sysctl_req *r) {
 (void)o;(void)l;(void)r;
 if(mode==0) {assert(strstr(s,"type_ok=64"));assert(strstr(s,"subtype_ok=64"));}
 if(mode==1) {assert(strstr(s,"type_ok=64"));assert(strstr(s,"subtype_prior=64"));}
 if(mode==2) {assert(strstr(s,"type_zero=64"));assert(strstr(s,"subtype_zero=64"));assert(strstr(s,"type_prior=0"));}
 if(mode==3) {assert(strstr(s,"type_errors=64 type_last_error=0x3"));assert(strstr(s,"subtype_errors=64 subtype_last_error=0x3"));}
 return output_error ? EIO : 0;
}
'''
main=r'''
int main(void){struct sysctl_req r={0}; assert(river_charge_alt_sysctl(0,0,0,&r)==ENXIO && writes==0);ri_power_ready=1;ri_power_busy=1;assert(river_charge_alt_sysctl(0,0,0,&r)==EBUSY && writes==0);ri_power_busy=0;for(mode=0;mode<4;mode++){writes=0;assert(river_charge_alt_sysctl(0,0,0,&r)==0 && writes==128 && !ri_power_busy);} mode=0;output_error=1;assert(river_charge_alt_sysctl(0,0,0,&r)==EIO&&!ri_power_busy);puts("PASS: correct, prior-register, zero, error-status, busy and output failure");}
'''
with tempfile.TemporaryDirectory(prefix='river-alt-') as d:
 src=Path(d)/'test.c';exe=Path(d)/'test';src.write_text(pre+fn+main);subprocess.run(['clang','-fsanitize=address,undefined',str(src),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
