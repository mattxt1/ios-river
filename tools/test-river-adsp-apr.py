#!/usr/bin/env python3
"""Test actual private SMEM/SMP2P and SMD APR code with modeled hardware."""
from pathlib import Path
import os
import argparse
import subprocess
import tempfile
repo=Path(__file__).resolve().parent.parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--baseline-0301',action='store_true',help='Reproduce the prior fixed-response framing restriction')
args=parser.parse_args()

def added(patch,name):
 p=(repo/'xnu/patches'/patch).read_text().split(f'diff --git a/bsd/dev/{name}',1)[1]
 s='\n'.join(x[1:] for x in p.splitlines() if x.startswith('+') and not x.startswith('+++'))+'\n'
 return s.replace('__asm__ volatile("dmb sy" ::: "memory");','host_barrier();')

collect=added('0300-river-adsp-private-transport-survey.patch','river_adsp_transport_survey.h').split('static bool ri_adsp_transport_truncated;',1)[0]
remote=added('0299-river-adsp-authenticated-start.patch','river_adsp_start.h').split('static struct {',1)[0]
apr=added('0301-river-adsp-apr-core-state.patch','river_adsp_apr.h')
with tempfile.TemporaryDirectory(prefix='river-adsp-apr-') as directory:
 root=Path(directory)
 if not args.baseline_0301:
  source=root/'source';target=source/'bsd/dev/river_adsp_apr.h'
  target.parent.mkdir(parents=True);target.write_text(apr)
  subprocess.run(['/usr/bin/patch','--batch','--silent','-p1','-d',str(source)],
   input=(repo/'xnu/patches/0302-river-apr-declared-response-framing.patch').read_bytes(),check=True)
  apr=target.read_text()
 apr=apr.split('static int\nriver_adsp_apr_sysctl',1)[0]
 apr=apr.replace('*(volatile uint8_t *)address=value;','host_write8(address,value);')
 for name,data in [('transport-collect-actual.h',collect),('remote-snapshot-actual.h',remote),('apr-actual.h',apr)]:
  (root/name).write_text(data)
 binary=root/'apr'
 subprocess.run([os.environ.get('CC','cc'),'-O1','-g','-Wall','-Wextra','-Werror',
  '-fsanitize=address,undefined','-DPATCHED_APR_FRAMING='+str(int(not args.baseline_0301)),'-I',str(root),str(repo/'tools/tests/river-adsp-apr-host.c'),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
