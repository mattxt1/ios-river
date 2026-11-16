#!/usr/bin/env python3
"""Actual physical-capture completion/stash/cleanup and codec rollback tests."""
from pathlib import Path
import subprocess,tempfile,os
repo=Path(__file__).resolve().parent.parent
patch=next((repo/'xnu/patches').glob('0331-*.patch')).read_text()
def added(name):
 s=patch.split('diff --git a/bsd/dev/'+name,1)[1].split('diff --git ',1)[0]
 return '\n'.join(x[1:] for x in s.splitlines() if x.startswith('+') and not x.startswith('+++'))+'\n'
with tempfile.TemporaryDirectory(prefix='river-mic-',dir=repo.parents[1]/'work') as d:
 root=Path(d)
 stash=added('river_mic_buffer.h').split('static int\nriver_mic_capture_sysctl')[0].replace('static int ri_mic_request(void);','')
 protocol=added('river_adsp_capture.h');a=protocol.index('static void\nri_mic_invalidate');b=protocol.index('static int\nri_mic_channel_command',a)
 protocol=protocol[:a]+'static void ri_mic_invalidate(vm_offset_t);\n'+protocol[b:]
 route=added('river_mic_route.h');a=route.index('static int\nri_mic_transport');b=route.index('static int\nri_mic_codec',a)
 codec=route[:a]+'static int ri_mic_transport(void);\n'+route[b:route.index('static void\nri_mic_worker')]
 source=root/'source';(source/'bsd/dev').mkdir(parents=True)
 p0=next((repo/'xnu/patches').glob('0270-*.patch')).read_text().split('diff --git a/bsd/dev/river_audio.c',1)[1].split('diff --git ',1)[0]
 (source/'bsd/dev/river_audio.c').write_text('\n'.join(x[1:] for x in p0.splitlines() if x.startswith('+') and not x.startswith('+++'))+'\n')
 for pattern in ['0273-*','0328-*','0330-*','0331-*']:
  sections=['diff --git '+x for x in next((repo/'xnu/patches').glob(pattern)).read_text().split('diff --git ')[1:]]
  text=''.join(x for x in sections if x.startswith('diff --git a/bsd/dev/river_audio.c '))
  subprocess.run(['patch','--batch','--silent','--fuzz=0','-p1','-d',str(source)],input=text.encode(),check=True)
 reader=(source/'bsd/dev/river_audio.c').read_text();reader=reader[reader.index('static int\nriver_mic_read'):reader.index('static int\nriver_mic_ioctl')]
 (root/'mic-reader-actual.h').write_text(reader)
 for n,s in [('mic-buffer-actual.h',stash),('mic-protocol-actual.h',protocol),('mic-codec-actual.h',codec)]:(root/n).write_text(s)
 subprocess.run([os.environ.get('CC','cc'),'-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=address,undefined','-I',d,str(repo/'tools/tests/river-microphone-host.c'),'-o',str(root/'test')],check=True)
 subprocess.run([str(root/'test')],check=True)
