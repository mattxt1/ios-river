#!/usr/bin/env python3
"""Exercise actual opt-in PCM staging, frame bounds, freezing and replay clamp."""
from pathlib import Path
import tempfile,subprocess,os
repo=Path(__file__).resolve().parent.parent
p=(repo/'xnu/patches/0326-river-stage-native-ios-pcm-for-bounded-playback.patch').read_text().split('diff --git a/bsd/dev/river_native_pcm.h',1)[1].split('diff --git ',1)[0]
s='\n'.join(x[1:] for x in p.splitlines() if x.startswith('+') and not x.startswith('+++'))+'\n';s=s.split('static int\nriver_native_pcm_stage_sysctl',1)[0]
with tempfile.TemporaryDirectory(prefix='river-native-pcm-') as d:
 root=Path(d);(root/'actual.h').write_text(s)
 subprocess.run([os.environ.get('CC','cc'),'-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-pthread','-I',d,str(repo/'tools/tests/river-native-pcm-host.c'),'-o',str(root/'test')],check=True)
 subprocess.run([str(root/'test')],check=True)
