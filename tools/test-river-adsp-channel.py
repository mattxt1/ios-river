#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile,os
repo=Path(__file__).resolve().parent.parent
def added(patch,name):
 s=(repo/'xnu/patches'/patch).read_text().split('diff --git a/bsd/dev/'+name,1)[1].split('diff --git ',1)[0]
 return '\n'.join(x[1:] for x in s.splitlines() if x.startswith('+') and not x.startswith('+++'))+'\n'
with tempfile.TemporaryDirectory(prefix='river-channel-') as d:
 root=Path(d);target=root/'source/bsd/dev/river_adsp_ngd.h';target.parent.mkdir(parents=True);target.write_text(added('0313-river-polled-ngd-capability-codec-address.patch','river_adsp_ngd.h'))
 patch=(repo/'xnu/patches/0314-river-muted-slimbus-channel-lifetime.patch').read_text().split('diff --git a/bsd/dev/river_adsp_channel.h',1)[0]
 subprocess.run(['patch','--batch','--silent','-p1','-d',str(root/'source')],input=patch.encode(),check=True)
 (root/'ngd-actual.h').write_text(target.read_text().split('static int\nriver_adsp_ngd_sysctl',1)[0].replace('__asm__ volatile("dmb sy" ::: "memory");','host_barrier();'))
 (root/'channel-actual.h').write_text(added('0314-river-muted-slimbus-channel-lifetime.patch','river_adsp_channel.h').split('static int\nriver_adsp_channel_sysctl',1)[0])
 p=root/'test';subprocess.run([os.environ.get('CC','cc'),'-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-value','-fsanitize=address,undefined','-I',d,str(repo/'tools/tests/river-adsp-channel-host.c'),'-o',str(p)],check=True);subprocess.run([str(p)],check=True)
