#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile,os
repo=Path(__file__).resolve().parent.parent
s=(repo/'xnu/patches/0313-river-polled-ngd-capability-codec-address.patch').read_text().split('diff --git a/bsd/dev/river_adsp_ngd.h',1)[1].split('diff --git ',1)[0]
s='\n'.join(x[1:] for x in s.splitlines() if x.startswith('+') and not x.startswith('+++'))+'\n';s=s.split('static int\nriver_adsp_ngd_sysctl',1)[0].replace('__asm__ volatile("dmb sy" ::: "memory");','host_barrier();')
with tempfile.TemporaryDirectory(prefix='river-ngd-') as d:
 root=Path(d);(root/'ngd-actual.h').write_text(s);p=root/'test'
 subprocess.run([os.environ.get('CC','cc'),'-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-value','-fsanitize=address,undefined','-I',d,str(repo/'tools/tests/river-adsp-ngd-host.c'),'-o',str(p)],check=True)
 subprocess.run([str(p)],check=True)
