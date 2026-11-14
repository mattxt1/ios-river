#!/usr/bin/env python3
"""Exercise the actual read-only private ADSP transport survey under sanitizers."""
from pathlib import Path
import os
import subprocess
import tempfile
repo=Path(__file__).resolve().parent.parent
patch=(repo/'xnu/patches/0300-river-adsp-private-transport-survey.patch').read_text()
part=patch.split('diff --git a/bsd/dev/river_adsp_transport_survey.h',1)[1]
header='\n'.join(line[1:] for line in part.splitlines() if line.startswith('+') and not line.startswith('+++'))+'\n'
header=header.replace('__asm__ volatile("dmb sy" ::: "memory");','host_barrier();')
with tempfile.TemporaryDirectory(prefix='river-adsp-transport-') as directory:
 root=Path(directory);(root/'adsp-transport-actual.h').write_text(header)
 binary=root/'adsp-transport'
 subprocess.run([os.environ.get('CC','cc'),'-O1','-g','-Wall','-Wextra','-Werror',
  '-fsanitize=address,undefined','-I',str(root),str(repo/'tools/tests/river-adsp-transport-host.c'),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
