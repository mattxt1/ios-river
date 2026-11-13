#!/usr/bin/env python3
"""Exercise the actual baseline and patched speaker helper with failed I2C I/O.

Requires an applied River kernel tree through patch0291 (0292 is also accepted).
All patch application and compilation take place in a temporary directory.
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--kernel-source', type=Path, required=True)
args = parser.parse_args()
repo = Path(__file__).resolve().parent.parent
source = (args.kernel_source / 'bsd/dev/sweet_keys.c').read_text()

def helper(text):
    start = text.index('static void\nriver_speaker_set_volume_locked(')
    end = text.index('\nvoid\nriver_speaker_set_volume(', start)
    return text[start:end] + '\n'

with tempfile.TemporaryDirectory(prefix='river-speaker-volume-') as directory:
    root = Path(directory)
    target = root / 'bsd/dev/sweet_keys.c'
    target.parent.mkdir(parents=True)
    target.write_text(source)
    subprocess.run(['git', 'apply', str(repo / 'xnu/patches/0293-river-speaker-volume-encoding.patch')], cwd=root, check=True)
    patched = target.read_text()
    for baseline, text in [(True, source), (False, patched)]:
        (root / 'volume-actual.h').write_text(helper(text))
        binary = root / ('baseline' if baseline else 'patched')
        command = [os.environ.get('CC', 'cc'), '-O2', '-Wall', '-Wextra', '-Werror', '-I', str(root)]
        if baseline:
            command += ['-DEXPECT_BASELINE']
        command += [str(repo / 'tools/tests/river-speaker-volume-host.c'), '-o', str(binary)]
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True)
