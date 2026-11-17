#!/usr/bin/env python3
"""Exercise actual automatic worker, guarded rearm and sticky failure state."""
from pathlib import Path
import subprocess,tempfile,os,argparse
parser=argparse.ArgumentParser();parser.add_argument('--microphone',action='store_true');args=parser.parse_args()
repo=Path(__file__).resolve().parent.parent;patches=repo/'xnu/patches'
def added(pattern,name):
 s=next(patches.glob(pattern)).read_text().split('diff --git a/bsd/dev/'+name,1)[1].split('diff --git ',1)[0]
 return '\n'.join(l[1:] for l in s.splitlines() if l.startswith('+') and not l.startswith('+++'))+'\n'
with tempfile.TemporaryDirectory(prefix='river-lifecycle-',dir=repo.parents[1]/'work') as d:
 root=Path(d);source=root/'source';(source/'bsd/dev').mkdir(parents=True)
 (source/'bsd/dev/river_audio_queue.h').write_text(added('0327-*','river_audio_queue.h'))
 for pattern in ['0328-*','0330-*']:
  sections=['diff --git '+x for x in next(patches.glob(pattern)).read_text().split('diff --git ')[1:]]
  text=''.join(x for x in sections if x.startswith('diff --git a/bsd/dev/river_audio_queue.h '))
  subprocess.run(['patch','--batch','--silent','--fuzz=0','-p1','-d',str(source)],input=text.encode(),check=True)
 s=(source/'bsd/dev/river_audio_queue.h').read_text().split('static int\nriver_audio_auto_sysctl')[0].replace('static int ri_audio_queue_launch(bool long_mode);','')
 (root/'queue-actual.h').write_text(s)
 target=source/'bsd/dev/river_audio_lifecycle.h';target.write_text(added('0330-*','river_audio_lifecycle.h'))
 if args.microphone:
  section='diff --git '+next(x for x in next(patches.glob('0335-*')).read_text().split('diff --git ')[1:] if x.startswith('a/bsd/dev/river_audio_lifecycle.h '))
  subprocess.run(['patch','--batch','--silent','--fuzz=0','-p1','-d',str(source)],input=section.encode(),check=True)
 (root/'lifecycle-actual.h').write_text(target.read_text())
 subprocess.run([os.environ.get('CC','cc'),*(['-DRIVER_MIC_HANDOFF_TEST'] if args.microphone else []),'-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=address,undefined','-I',d,str(repo/'tools/tests/river-audio-lifecycle-host.c'),'-o',str(root/'test')],check=True)
 subprocess.run([str(root/'test')],check=True)
