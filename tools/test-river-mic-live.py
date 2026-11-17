#!/usr/bin/env python3
"""Exercise actual physical ring with concurrent producer/reader and teardown."""
from pathlib import Path
import subprocess,tempfile,os
repo=Path(__file__).resolve().parent.parent
patch=next((repo/'xnu/patches').glob('0335-*.patch')).read_text()
def added(name):
 section=next(x for x in patch.split('diff --git ')[1:] if x.startswith('a/bsd/dev/'+name+' '))
 return '\n'.join(x[1:] for x in section.splitlines() if x.startswith('+') and not x.startswith('+++'))+'\n'
with tempfile.TemporaryDirectory(prefix='river-mic-live-',dir=repo.parents[1]/'work') as d:
 root=Path(d);text=added('river_mic_live.h').split('static int river_mic_live_sysctl')[0]
 (root/'mic-live-actual.h').write_text(text)
 subprocess.run([os.environ.get('CC','cc'),'-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=address,undefined','-pthread','-I',d,str(repo/'tools/tests/river-mic-live-host.c'),'-o',str(root/'test')],check=True)
 subprocess.run([str(root/'test')],check=True)
