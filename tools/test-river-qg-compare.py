from pathlib import Path
import subprocess, sys, tempfile
if len(sys.argv) != 2:
    raise SystemExit('usage: test-river-qg-compare.py PATCHED_SWEET_KEYS_C')
s = Path(sys.argv[1]).read_text()
a = s.index('static int\nriver_qg_compare_sysctl')
b = s.index('\nSYSCTL_PROC(_debug, OID_AUTO, river_qg_compare', a)
fn = s[a:b]
pre = r'''#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
typedef uintptr_t vm_offset_t;
struct sysctl_req { int unused; };
#define SYSCTL_HANDLER_ARGS (void *oidp, void *arg1, int arg2, struct sysctl_req *req)
static int ri_power_ready, ri_power_busy, calls, failat, output_error;
static vm_offset_t ri_power_qg=1;
static const unsigned regs[]={4,5,8,0xc0,0xc0,0xc2,0xc6,0xc0};
static const unsigned sizes[]={1,1,1,4,2,2,2,4};
static bool ri_power_read(vm_offset_t p,unsigned reg,unsigned size,uint32_t *out) {
 assert(p==1 && calls<8 && reg==regs[calls] && size==sizes[calls]);
 calls++; if(calls==failat) return false; *out=reg; return true;
}
static int sysctl_handle_string(void *o,char *s,int n,struct sysctl_req *r) {
 (void)o;(void)n;(void)r; assert(strstr(s,"status1=0x8") && strstr(s,"burst_unheld=0xc6")); return output_error;
}
'''
main = r'''int main(void) {
 struct sysctl_req r={0};
 assert(river_qg_compare_sysctl(0,0,0,&r)==ENXIO && !calls);
 ri_power_ready=1; ri_power_busy=1;
 assert(river_qg_compare_sysctl(0,0,0,&r)==EBUSY && !calls);
 ri_power_busy=0;
 for (int n=1;n<=8;n++) { calls=0;failat=n;assert(river_qg_compare_sysctl(0,0,0,&r)==EIO && calls==n && !ri_power_busy); }
 calls=0;failat=0;output_error=EFAULT;
 assert(river_qg_compare_sysctl(0,0,0,&r)==EFAULT && calls==8 && !ri_power_busy);
 calls=0;output_error=0;assert(!river_qg_compare_sysctl(0,0,0,&r) && calls==8 && !ri_power_busy);
 puts("PASS: read sequence, unavailable, contention, every read error and output failure");
}'''
with tempfile.TemporaryDirectory(prefix='river-qg-compare-') as d:
    src=Path(d)/'test.c'; exe=Path(d)/'test'; src.write_text(pre+fn+main)
    subprocess.run(['clang','-fsanitize=address,undefined',str(src),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
