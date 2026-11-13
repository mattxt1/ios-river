#!/usr/bin/env python3
"""Test actual patched RPM transport with bounded hardware models and sanitizers."""
from pathlib import Path
import os
import subprocess
import tempfile
repo=Path(__file__).resolve().parent.parent

def header(patch, name):
    text=(repo/'xnu/patches'/patch).read_text().split(f'diff --git a/bsd/dev/{name}',1)[1]
    value='\n'.join(line[1:] for line in text.splitlines() if line.startswith('+') and not line.startswith('+++'))+'\n'
    return value.replace('__asm__ volatile("dmb sy" ::: "memory");','host_barrier();')

with tempfile.TemporaryDirectory(prefix='river-rpm-transport-') as directory:
    root=Path(directory)
    (root/'rpm-reply-actual.h').write_text(header('0294-river-rpm-exact-replies.patch','river_rpm_reply.h'))
    (root/'rpm-transport-actual.h').write_text(header('0298-river-rpm-shared-transport.patch','river_rpm_transport.h'))
    binary=root/'rpm-transport'
    subprocess.run([os.environ.get('CC','cc'),'-O1','-g','-Wall','-Wextra','-Werror',
        '-fsanitize=address,undefined','-I',str(root),str(repo/'tools/tests/river-rpm-transport-host.c'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
