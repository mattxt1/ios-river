#!/usr/bin/env python3
"""Test actual one-shot ADSP startup using bounded SMEM/RPM/PAS hardware models."""
from pathlib import Path
import os
import subprocess
import tempfile
repo=Path(__file__).resolve().parent.parent
patch=(repo/'xnu/patches/0299-river-adsp-authenticated-start.patch').read_text()
part=patch.split('diff --git a/bsd/dev/river_adsp_start.h',1)[1]
header='\n'.join(line[1:] for line in part.splitlines() if line.startswith('+') and not line.startswith('+++'))+'\n'
header=header.split('static int\nriver_adsp_start_sysctl',1)[0]
header=header.replace('__asm__ volatile("dmb sy" ::: "memory");','host_barrier();').replace('__asm__ volatile("dsb sy" ::: "memory");','host_barrier();')
with tempfile.TemporaryDirectory(prefix='river-adsp-start-') as directory:
 root=Path(directory);(root/'adsp-start-actual.h').write_text(header)
 binary=root/'adsp-start'
 subprocess.run([os.environ.get('CC','cc'),'-O1','-g','-Wall','-Wextra','-Werror',
  '-fsanitize=address,undefined','-I',str(root),str(repo/'tools/tests/river-adsp-start-host.c'),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
