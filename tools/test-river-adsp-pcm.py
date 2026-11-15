#!/usr/bin/env python3
"""Test actual shared PCM mapping and ASM/AFE transactions with modeled hardware."""
from pathlib import Path
import os
import argparse
import subprocess
import tempfile
repo=Path(__file__).resolve().parent.parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--baseline-0301',action='store_true',help='Reproduce the prior fixed-response framing restriction')
parser.add_argument('--with-routing',action='store_true',help='Apply the0304 PCM transport diagnostics before testing')
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
 pcm=added('0303-river-pcm-map-asm-afe-prerequisites.patch','river_adsp_pcm.h').split('static int\nriver_adsp_pcm_sysctl',1)[0]
 if args.with_routing:
  target=root/'source/bsd/dev/river_adsp_pcm.h';target.parent.mkdir(parents=True,exist_ok=True);target.write_text(pcm)
  patches=(repo/'xnu/patches/0304-river-afe-config-adm-matrix-routing.patch').read_text()
  section='diff --git a/bsd/dev/river_adsp_pcm.h'+patches.split('diff --git a/bsd/dev/river_adsp_pcm.h',1)[1].split('diff --git ',1)[0]
  subprocess.run(['/usr/bin/patch','--batch','--silent','-p1','-d',str(root/'source')],input=section.encode(),check=True)
  pcm=target.read_text()
 target=root/'source/bsd/dev/river_adsp_pcm.h';target.parent.mkdir(parents=True,exist_ok=True);target.write_text(pcm)
 sections=['diff --git '+x for x in (repo/'xnu/patches/0321-river-bounded-quiet-codec-pcm-tone.patch').read_text().split('diff --git ')[1:]]
 text=''.join(x for x in sections if x.splitlines()[0].split()[2]=='a/bsd/dev/river_adsp_pcm.h')
 subprocess.run(['patch','--batch','--silent','--fuzz=0','-p1','-d',str(root/'source')],input=text.encode(),check=True)
 pcm=target.read_text()
 (root/'pcm-actual.h').write_text(pcm)
 (root/'pcm-stubs.h').write_text((repo/'tools/tests/river-adsp-pcm-stubs.h').read_text())
 apr=apr.replace('*(volatile uint8_t *)address=value;','host_write8(address,value);')
 for name,data in [('transport-collect-actual.h',collect),('remote-snapshot-actual.h',remote),('apr-actual.h',apr)]:
  (root/name).write_text(data)
 binary=root/'apr'
 subprocess.run([os.environ.get('CC','cc'),'-O1','-g','-Wall','-Wextra','-Werror',
  '-fsanitize=address,undefined','-DPATCHED_APR_FRAMING='+str(int(not args.baseline_0301)),'-I',str(root),str(repo/'tools/tests/river-adsp-pcm-host.c'),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
