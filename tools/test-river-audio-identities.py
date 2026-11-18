#!/usr/bin/env python3
"""Exercise actual speaker and input channel commands beyond 8-bit wire IDs."""
from pathlib import Path
import subprocess,tempfile,os
from river_audio_test_source import source
repo=Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix='river-audio-identities-',dir=repo.parents[1]/'work') as d:
 root=Path(d)
 for name,begin,end in [('channel','static int\nri_channel_command','static void\nri_channel_cleanup'),('capture','static int\nri_mic_channel_command','static void\nri_mic_dsp_cleanup')]:
  s=source('river_adsp_'+name+'.h');(root/(name+'-actual.h')).write_text(s[s.index(begin):s.index(end)])
 subprocess.run([os.environ.get('CC','cc'),'-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',d,str(repo/'tools/tests/river-audio-identities-host.c'),'-o',str(root/'test')],check=True)
 subprocess.run([str(root/'test')],check=True)
