#!/usr/bin/env python3
"""Run actual microphone lifecycle/rearm code with hardware ownership model."""
from pathlib import Path
import subprocess,tempfile,os,argparse
parser=argparse.ArgumentParser();parser.add_argument('--restart',action='store_true');parser.add_argument('--ios',action='store_true');args=parser.parse_args();args.restart=args.restart or args.ios
from river_audio_test_source import source as actual_source
repo=Path(__file__).resolve().parent.parent
p=next((repo/'xnu/patches').glob('0335-*')).read_text();section=next(x for x in p.split('diff --git ')[1:] if x.startswith('a/bsd/dev/river_mic_lifecycle.h '));s='\n'.join(x[1:] for x in section.splitlines() if x.startswith('+') and not x.startswith('+++'))+'\n'
with tempfile.TemporaryDirectory(prefix='river-mic-lifecycle-',dir=repo.parents[1]/'work') as d:
 root=Path(d)
 if args.restart:
  path=root/'bsd/dev/river_mic_lifecycle.h';path.parent.mkdir(parents=True);path.write_text(s)
  section='diff --git '+next(x for x in next((repo/'xnu/patches').glob('0336-*')).read_text().split('diff --git ')[1:] if x.startswith('a/bsd/dev/river_mic_lifecycle.h '))
  subprocess.run(['patch','--batch','--silent','--fuzz=0','-p1','-d',d],input=section.encode(),check=True);s=path.read_text()
 if args.ios:s=actual_source('river_mic_lifecycle.h')
 (root/'mic-lifecycle-actual.h').write_text(s)
 subprocess.run([os.environ.get('CC','cc'),*(['-DRIVER_IOS_AUDIO'] if args.ios else []),*(['-DRIVER_MIC_RESTART_TEST'] if args.restart else []),'-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=address,undefined','-I',d,str(repo/'tools/tests/river-mic-lifecycle-host.c'),'-o',str(root/'test')],check=True)
 subprocess.run([str(root/'test')],check=True)
