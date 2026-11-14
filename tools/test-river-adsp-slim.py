#!/usr/bin/env python3
"""Test actual IPC-router/QMI control code with modeled CID2 and DSP replies."""
from pathlib import Path
import subprocess,tempfile,os
repo=Path(__file__).resolve().parent.parent
s=(repo/'xnu/patches/0309-river-slimbus-ipcrtr-qmi-prerequisites.patch').read_text().split('diff --git a/bsd/dev/river_adsp_slim.h',1)[1].split('diff --git ',1)[0]
s='\n'.join(x[1:] for x in s.splitlines() if x.startswith('+') and not x.startswith('+++'))+'\n';s=s.split('static int\nriver_adsp_slim_sysctl',1)[0].replace('__asm__ volatile("dmb sy" ::: "memory");','host_barrier();')
with tempfile.TemporaryDirectory(prefix='river-slim-') as d:
 root=Path(d);target=root/'source/bsd/dev/river_adsp_slim.h';target.parent.mkdir(parents=True);target.write_text(s)
 subprocess.run(['patch','--batch','--silent','-p1','-d',str(root/'source')],input=(repo/'xnu/patches/0310-river-ipcrtr-broadcast-control-replies.patch').read_bytes(),check=True)
 subprocess.run(['patch','--batch','--silent','-p1','-d',str(root/'source')],input=(repo/'xnu/patches/0311-river-qrtr-firmware-control-endpoint.patch').read_bytes(),check=True)
 (root/'slim-actual.h').write_text(target.read_text());p=root/'test'
 subprocess.run([os.environ.get('CC','cc'),'-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-value','-Wno-unused-function','-fsanitize=address,undefined','-I',d,str(repo/'tools/tests/river-adsp-slim-host.c'),'-o',str(p)],check=True)
 subprocess.run([str(p)],check=True)
