#!/usr/bin/env python3
"""Exercise the actual bounded RPM reply receiver with sanitizers, without hardware."""
from pathlib import Path
import os
import subprocess
import tempfile

repo = Path(__file__).resolve().parent.parent
patch = (repo/'xnu/patches/0294-river-rpm-exact-replies.patch').read_text()
added = patch.split('diff --git a/bsd/dev/river_rpm_reply.h',1)[1]
header = '\n'.join(line[1:] for line in added.splitlines() if line.startswith('+') and not line.startswith('+++'))+'\n'
header = header.replace('__asm__ volatile("dmb sy" ::: "memory");','host_barrier();')
with tempfile.TemporaryDirectory(prefix='river-rpm-reply-') as directory:
    root = Path(directory)
    (root/'rpm-reply-actual.h').write_text(header)
    binary = root/'rpm-reply'
    subprocess.run([os.environ.get('CC','cc'),'-O1','-g','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined','-I',str(root),
                    str(repo/'tools/tests/river-rpm-reply-host.c'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
