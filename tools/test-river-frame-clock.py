#!/usr/bin/env python3
"""Exercise the actual fixed-deadline display clock from the River patch."""
from pathlib import Path
import subprocess,tempfile,os
repo=Path(__file__).resolve().parent.parent
patch=next((repo/'xnu/patches').glob('0339-*')).read_text()
section=next(x for x in patch.split('diff --git ')[1:] if x.startswith('a/iokit/Kernel/arm/river_frame_clock.h '))
s='\n'.join(x[1:] for x in section.splitlines() if x.startswith('+') and not x.startswith('+++'))+'\n'
with tempfile.TemporaryDirectory(prefix='river-frame-clock-',dir=repo.parents[1]/'work') as d:
 root=Path(d);(root/'river_frame_clock.h').write_text(s)
 subprocess.run([os.environ.get('CC','cc'),'-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',d,str(repo/'tools/tests/river-frame-clock-host.c'),'-o',str(root/'test')],check=True)
 subprocess.run([str(root/'test')],check=True)
