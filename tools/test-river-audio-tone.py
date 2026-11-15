#!/usr/bin/env python3
"""Exercise actual amplifier tone gate, deadline mute and cleanup code."""
from pathlib import Path
import tempfile,subprocess,os
repo=Path(__file__).resolve().parent.parent
s=(repo/'xnu/patches/0321-river-bounded-quiet-codec-pcm-tone.patch').read_text().split('diff --git a/bsd/dev/river_adsp_tone.h',1)[1].split('diff --git ',1)[0]
s='\n'.join(x[1:] for x in s.splitlines() if x.startswith('+') and not x.startswith('+++'))+'\n'
with tempfile.TemporaryDirectory(prefix='river-quiet-tone-') as d:
 root=Path(d);source=root/'source';target=source/'bsd/dev/river_adsp_tone.h';target.parent.mkdir(parents=True);target.write_text(s)
 sections=['diff --git '+x for x in (repo/'xnu/patches/0324-river-align-amplifier-receive-slot-with-codec.patch').read_text().split('diff --git ')[1:]]
 text=''.join(x for x in sections if x.splitlines()[0].split()[2]=='a/bsd/dev/river_adsp_tone.h')
 subprocess.run(['patch','--batch','--silent','--fuzz=0','-p1','-d',str(source)],input=text.encode(),check=True)
 sections=['diff --git '+x for x in (repo/'xnu/patches/0325-river-mute-immediately-after-rendered-eos.patch').read_text().split('diff --git ')[1:]]
 text=''.join(x for x in sections if x.splitlines()[0].split()[2]=='a/bsd/dev/river_adsp_tone.h')
 subprocess.run(['patch','--batch','--silent','--fuzz=0','-p1','-d',str(source)],input=text.encode(),check=True)
 sections=['diff --git '+x for x in (repo/'xnu/patches/0327-river-live-pcm-queue-and-dsp-pipeline.patch').read_text().split('diff --git ')[1:]]
 text=''.join(x for x in sections if x.startswith('diff --git a/bsd/dev/river_adsp_tone.h '))
 subprocess.run(['patch','--batch','--silent','--fuzz=0','-p1','-d',str(source)],input=text.encode(),check=True)
 (root/'tone-actual.h').write_text(target.read_text());binary=root/'tone' 
 subprocess.run([os.environ.get('CC','cc'),'-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',d,str(repo/'tools/tests/river-audio-tone-host.c'),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
