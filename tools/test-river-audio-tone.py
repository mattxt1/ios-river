#!/usr/bin/env python3
"""Exercise actual amplifier tone gate, deadline mute and cleanup code."""
from pathlib import Path
import tempfile,subprocess,os
repo=Path(__file__).resolve().parent.parent
s=(repo/'xnu/patches/0321-river-bounded-quiet-codec-pcm-tone.patch').read_text().split('diff --git a/bsd/dev/river_adsp_tone.h',1)[1].split('diff --git ',1)[0]
s='\n'.join(x[1:] for x in s.splitlines() if x.startswith('+') and not x.startswith('+++'))+'\n'
with tempfile.TemporaryDirectory(prefix='river-quiet-tone-') as d:
 root=Path(d);(root/'tone-actual.h').write_text(s);binary=root/'tone'
 subprocess.run([os.environ.get('CC','cc'),'-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',d,str(repo/'tools/tests/river-audio-tone-host.c'),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
