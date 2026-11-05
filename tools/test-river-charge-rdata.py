from pathlib import Path
import subprocess, sys, tempfile

if len(sys.argv) != 2:
    raise SystemExit('usage: test-river-charge-rdata.py PATCHED_SWEET_KEYS_C')
s = Path(sys.argv[1]).read_text()
a = s.index('static int\nriver_charge_rdata_sysctl')
b = s.index('\nSYSCTL_PROC(_debug, OID_AUTO, river_charge_rdata', a)
fn = s[a:b]
pre = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
typedef uintptr_t vm_offset_t;
struct sysctl_req { int unused; };
#define SYSCTL_HANDLER_ARGS (void *oidp, void *arg1, int arg2, struct sysctl_req *req)
static int ri_power_ready, ri_power_busy, mode, reads, calls, output_error;
static vm_offset_t ri_charge_chgr=1;
static bool ri_charge_timing_read(vm_offset_t p, unsigned d, uint32_t *s, uint32_t *v) {
 assert(p==1 && d==0); *s=1; calls++; *v=(mode==1?0:2); return mode!=2;
}
static uint32_t ri_rd(vm_offset_t p, unsigned reg) {
 assert(p==1 && reg==0x18); reads++; return mode==1 && (reads%2)==0 ? 2 : (mode==1?0:2);
}
static int sysctl_handle_string(void *o,char *s,int l,struct sysctl_req *r) {
 (void)o;(void)l;(void)r;
 if(mode==0) { assert(strstr(s,"first_two=64")); assert(strstr(s,"changed=0")); }
 if(mode==1) { assert(strstr(s,"first_zero=64")); assert(strstr(s,"changed=64")); assert(strstr(s,"changed_includes_zero=64")); }
 if(mode==2) { assert(strstr(s,"errors=64")); assert(strstr(s,"changed=0")); }
 return output_error ? EIO : 0;
}
'''
main = r'''
int main(void) {
 struct sysctl_req r={0};
 assert(river_charge_rdata_sysctl(0,0,0,&r)==ENXIO && calls==0);
 ri_power_ready=1; ri_power_busy=1;
 assert(river_charge_rdata_sysctl(0,0,0,&r)==EBUSY && calls==0);
 ri_power_busy=0;
 for(mode=0;mode<3;mode++) {
  reads=calls=0;
  assert(river_charge_rdata_sysctl(0,0,0,&r)==0 && calls==64 && !ri_power_busy);
  assert(reads==(mode==2?0:128));
 }
 mode=0; output_error=1;
 assert(river_charge_rdata_sysctl(0,0,0,&r)==EIO && !ri_power_busy);
 puts("PASS: stable, changing, transaction failures, unavailable, busy and output error");
}
'''
with tempfile.TemporaryDirectory(prefix='river-rdata-') as d:
    src = Path(d)/'test.c'; exe = Path(d)/'test'; src.write_text(pre+fn+main)
    subprocess.run(['clang','-fsanitize=address,undefined',str(src),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
