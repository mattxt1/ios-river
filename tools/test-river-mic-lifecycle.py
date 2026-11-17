#!/usr/bin/env python3
"""Run actual microphone lifecycle/rearm code with hardware ownership model."""
from pathlib import Path
import subprocess,tempfile,os
repo=Path(__file__).resolve().parent.parent
p=next((repo/'xnu/patches').glob('0335-*')).read_text();section=next(x for x in p.split('diff --git ')[1:] if x.startswith('a/bsd/dev/river_mic_lifecycle.h '));s='\n'.join(x[1:] for x in section.splitlines() if x.startswith('+') and not x.startswith('+++'))+'\n'
with tempfile.TemporaryDirectory(prefix='river-mic-lifecycle-',dir=repo.parents[1]/'work') as d:
 root=Path(d);(root/'mic-lifecycle-actual.h').write_text(s)
 subprocess.run([os.environ.get('CC','cc'),'-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=address,undefined','-I',d,str(repo/'tools/tests/river-mic-lifecycle-host.c'),'-o',str(root/'test')],check=True)
 subprocess.run([str(root/'test')],check=True)
