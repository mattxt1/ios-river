#!/usr/bin/env python3
"""Actual device-write uio/reservation semantics, with no DSP/hardware waits."""
from pathlib import Path
import subprocess,tempfile,os
repo=Path(__file__).resolve().parent.parent;patches=repo/'xnu/patches'
def added(pattern,name):
 p=next(patches.glob(pattern)).read_text().split('diff --git a/bsd/dev/'+name,1)[1].split('diff --git ',1)[0]
 return '\n'.join(l[1:] for l in p.splitlines() if l.startswith('+') and not l.startswith('+++'))+'\n'
with tempfile.TemporaryDirectory(prefix='river-writer-',dir=repo.parents[1]/'work') as d:
 root=Path(d);source=root/'source';(source/'bsd/dev').mkdir(parents=True)
 (source/'bsd/dev/river_audio.c').write_text(added('0270-*','river_audio.c'))
 (source/'bsd/dev/river_audio_queue.h').write_text(added('0327-*','river_audio_queue.h'))
 for pattern in ['0273-*','0328-*']:
  sections=['diff --git '+x for x in next(patches.glob(pattern)).read_text().split('diff --git ')[1:]]
  text=''.join(x for x in sections if x.splitlines()[0].split()[2] in ['a/bsd/dev/river_audio.c','a/bsd/dev/river_audio_queue.h'])
  subprocess.run(['patch','--batch','--silent','--fuzz=0','-p1','-d',str(source)],input=text.encode(),check=True)
 s=(source/'bsd/dev/river_audio.c').read_text();s=s[s.index('static int\nriver_audio_write'):s.index('static int\nriver_audio_ioctl')]
 queue=(source/'bsd/dev/river_audio_queue.h').read_text().split('static int\nriver_audio_queue_sysctl')[0].replace('static int ri_audio_queue_launch(bool long_mode);','')
 (root/'writer-actual.h').write_text(s);(root/'queue-actual.h').write_text(queue)
 subprocess.run([os.environ.get('CC','cc'),'-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-pthread','-I',d,str(repo/'tools/tests/river-audio-writer-host.c'),'-o',str(root/'test')],check=True)
 subprocess.run([str(root/'test')],check=True)
