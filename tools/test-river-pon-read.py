from pathlib import Path
import subprocess, sys, tempfile
if len(sys.argv)!=2: raise SystemExit('usage: test-river-pon-read.py PATCHED_SWEET_KEYS_C')
s=Path(sys.argv[1]).read_text(); a=s.index('static bool\nri_pon(uint8_t *v)'); b=s.index('\n/*',a); fn=s[a:b]
pre=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
static int ri_pon_ok, mode, calls; static uint32_t ri_pon_started,ri_pon_finished,ri_pon_deferred; static int ri_pon_survey_gate; static uintptr_t ri_obs=1;
static void ri_wr(uintptr_t b,unsigned o,uint32_t v){assert(b==1&&o==0&&v==((1u<<27)|0x100));}
static uint32_t ri_rd(uintptr_t b,unsigned o){assert(b==1);if(o==0x18)return 0x42;assert(o==8);calls++;return mode==0?1:mode==1?9:0;}
static void delay(unsigned n){assert(n==1);}
'''
main=r'''
int main(void){uint8_t v=0;assert(!ri_pon(&v)&&!ri_pon_started);ri_pon_ok=1;for(mode=0;mode<3;mode++){calls=0;bool ok=ri_pon(&v);assert(ok==(mode==0));if(ok)assert(v==0x42);if(mode==2)assert(calls==1000);assert(ri_pon_started==ri_pon_finished);}ri_pon_survey_gate=1; assert(!ri_pon(&v)&&ri_pon_deferred==1); puts("PASS: PON success, error, timeout, balanced counters and busy gate");}
'''
with tempfile.TemporaryDirectory(prefix='river-pon-read-') as d:
 src=Path(d)/'test.c'; exe=Path(d)/'test'; src.write_text(pre+fn+main); subprocess.run(['clang','-fsanitize=address,undefined',str(src),'-o',str(exe)],check=True); subprocess.run([str(exe)],check=True)
