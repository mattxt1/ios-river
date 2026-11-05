from pathlib import Path
import subprocess, sys, tempfile
if len(sys.argv)!=2: raise SystemExit('usage: test-river-spmi-gate.py PATCHED_SWEET_KEYS_C')
s=Path(sys.argv[1]).read_text()
def extract(start,end):
 a=s.index(start);return s[a:s.index(end,a)]
gate=extract('static int ri_spmi_gate, ri_spmi_poisoned;', 'static inline uint32_t ri_fifo_rd32')
read=extract('static bool\nri_spmi_read(vm_offset_t obs, unsigned addr, unsigned count, uint32_t *v)\n{','\nbool river_rtc_utc')
alt=extract('static bool\nri_charge_alt_read(', '\nstatic int\nriver_charge_alt_sysctl')
timing=extract('static bool\nri_charge_timing_read(', '\nstatic int\nriver_charge_timing_sysctl')
write=extract('static bool\nri_spmi_write(', '\n/* Read back every reset')
pre=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <sched.h>
typedef uintptr_t vm_offset_t;
static int mode, mmio_active, collisions, commands, last_write;
static void delay(unsigned n){(void)n;sched_yield();}
static void ri_wr(vm_offset_t base,unsigned off,uint32_t v){(void)base;(void)v;if(off==0){last_write=(v & (1u<<27))==0;if(__atomic_exchange_n(&mmio_active,1,__ATOMIC_SEQ_CST))__atomic_add_fetch(&collisions,1,__ATOMIC_SEQ_CST);__atomic_add_fetch(&commands,1,__ATOMIC_SEQ_CST);}}
static uint32_t ri_rd(vm_offset_t base,unsigned off){(void)base;if(off==8){if(mode==2)return 0;if(mode==1){__atomic_store_n(&mmio_active,0,__ATOMIC_SEQ_CST);return 9;}if(last_write)__atomic_store_n(&mmio_active,0,__ATOMIC_SEQ_CST);return 1;}assert(off==0x18);__atomic_store_n(&mmio_active,0,__ATOMIC_SEQ_CST);return 2;}
'''
main=r'''
static void *worker(void *p){(void)p;for(int i=0;i<10000;i++){uint32_t v=99;if(ri_spmi_read(1,4,1,&v))assert(v==2);}return NULL;}
int main(void){uint32_t v=99,s=0;pthread_t a,b;assert(!pthread_create(&a,0,worker,0));assert(!pthread_create(&b,0,worker,0));pthread_join(a,0);pthread_join(b,0);assert(!collisions&&!ri_spmi_gate&&commands>0);assert(ri_charge_alt_read(1,4,&s,&v)&&s==1&&v==2);assert(ri_charge_timing_read(1,0,&s,&v)&&s==1&&v==2);assert(ri_spmi_write(1,4,7)&&!ri_spmi_gate&&!ri_spmi_poisoned);ri_spmi_gate=1;int busy_before=commands;assert(!ri_spmi_read(1,4,1,&v)&&commands==busy_before);ri_spmi_gate=0;mode=1;v=99;assert(!ri_spmi_read(1,4,1,&v)&&v==99&&!ri_spmi_poisoned&&!ri_spmi_gate);assert(!ri_charge_alt_read(1,4,&s,&v)&&s==9&&!ri_spmi_poisoned);assert(!ri_spmi_write(1,4,7)&&!ri_spmi_poisoned&&!ri_spmi_gate);mode=2;assert(!ri_spmi_read(1,4,1,&v)&&ri_spmi_poisoned&&!ri_spmi_gate);int before=commands;assert(!ri_spmi_read(1,4,1,&v)&&!ri_charge_alt_read(1,4,&s,&v)&&!ri_spmi_write(1,4,7)&&commands==before);ri_spmi_poisoned=0;mmio_active=0;assert(!ri_charge_alt_read(1,4,&s,&v)&&ri_spmi_poisoned&&!ri_spmi_gate);ri_spmi_poisoned=0;mmio_active=0;assert(!ri_charge_timing_read(1,0,&s,&v)&&ri_spmi_poisoned&&!ri_spmi_gate);ri_spmi_poisoned=0;mmio_active=0;assert(!ri_spmi_write(1,4,7)&&ri_spmi_poisoned&&!ri_spmi_gate);puts("PASS: concurrent exclusion, read paths, failures, timeout poisoning and lock release");}
'''
with tempfile.TemporaryDirectory(prefix='river-spmi-gate-') as d:
 src=Path(d)/'test.c';exe=Path(d)/'test';src.write_text(pre+gate+read+alt+timing+write+main);subprocess.run(['clang','-pthread','-fsanitize=address,undefined',str(src),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
