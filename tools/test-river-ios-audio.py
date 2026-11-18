#!/usr/bin/env python3
"""Actual normal-mode coordinator with mocked hardware session boundaries."""
from pathlib import Path
import subprocess,tempfile,os
repo=Path(__file__).resolve().parent.parent
p=next((repo/'xnu/patches').glob('0337-*')).read_text()
section=next(x for x in p.split('diff --git ')[1:] if x.startswith('a/bsd/dev/river_audio_normal.h '))
s='\n'.join(x[1:] for x in section.splitlines() if x.startswith('+') and not x.startswith('+++'))+'\n'
s=s.split('static int river_audio_normal_sysctl')[0]
with tempfile.TemporaryDirectory(prefix='river-ios-audio-',dir=repo.parents[1]/'work') as d:
 root=Path(d);(root/'normal-actual.h').write_text(s)
 subprocess.run([os.environ.get('CC','cc'),'-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=address,undefined','-I',d,str(repo/'tools/tests/river-ios-audio-host.c'),'-o',str(root/'test')],check=True)
 subprocess.run([str(root/'test')],check=True)
