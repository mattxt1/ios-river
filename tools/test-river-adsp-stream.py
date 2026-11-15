#!/usr/bin/env python3
"""Compile actual zero-PCM DMA/completion state with modeled DSP and allocation."""
from pathlib import Path
import subprocess,tempfile,os,argparse
parser=argparse.ArgumentParser(description=__doc__)
args=parser.parse_args()
repo=Path(__file__).resolve().parent.parent
patch=(repo/'xnu/patches/0304-river-afe-config-adm-matrix-routing.patch').read_text()
p=patch.split('diff --git a/bsd/dev/river_adsp_route.h',1)[1].split('diff --git ',1)[0]
source='\n'.join(x[1:] for x in p.splitlines() if x.startswith('+') and not x.startswith('+++'))+'\n'
source=source.split('static int\nriver_adsp_route_sysctl',1)[0]
with tempfile.TemporaryDirectory(prefix='river-adsp-route-') as directory:
 root=Path(directory)
 target=root/'source/bsd/dev/river_adsp_route.h';target.parent.mkdir(parents=True);target.write_text(source)
 subprocess.run(['patch','--batch','--silent','-p1','-d',str(root/'source')],input=(repo/'xnu/patches/0305-river-afe-get-capacity-record-bounds.patch').read_bytes(),check=True)
 stream_patch=(repo/'xnu/patches/0306-river-zero-pcm-write-completion.patch').read_text().split('diff --git a/bsd/dev/river_adsp_stream.h',1)[1].split('diff --git ',1)[0]
 stream='\n'.join(x[1:] for x in stream_patch.splitlines() if x.startswith('+') and not x.startswith('+++'))+'\n'
 stream_target=root/'source/bsd/dev/river_adsp_stream.h';stream_target.write_text(stream)
 subprocess.run(['patch','--batch','--silent','-p1','-d',str(root/'source')],input=(repo/'xnu/patches/0307-river-afe-reconfigure-before-pcm-start.patch').read_bytes(),check=True)
 subprocess.run(['patch','--batch','--silent','-p1','-d',str(root/'source')],input=(repo/'xnu/patches/0308-river-combine-first-afe-start-with-pcm.patch').read_bytes(),check=True)
 sections=['diff --git '+x for x in (repo/'xnu/patches/0317-river-muted-pcm-through-active-codec-route.patch').read_text().split('diff --git ')[1:]]
 text=''.join(x for x in sections if x.splitlines()[0].split()[2]=='a/bsd/dev/river_adsp_stream.h')
 subprocess.run(['patch','--batch','--silent','--fuzz=0','-p1','-d',str(root/'source')],input=text.encode(),check=True)
 sections=['diff --git '+x for x in (repo/'xnu/patches/0318-river-prepare-afe-before-codec-channel.patch').read_text().split('diff --git ')[1:]]
 text=''.join(x for x in sections if x.splitlines()[0].split()[2] in ['a/bsd/dev/river_adsp_stream.h'])
 subprocess.run(['patch','--batch','--silent','--fuzz=0','-p1','-d',str(root/'source')],input=text.encode(),check=True)
 sections=['diff --git '+x for x in (repo/'xnu/patches/0321-river-bounded-quiet-codec-pcm-tone.patch').read_text().split('diff --git ')[1:]]
 text=''.join(x for x in sections if x.splitlines()[0].split()[2]=='a/bsd/dev/river_adsp_stream.h')
 subprocess.run(['patch','--batch','--silent','--fuzz=0','-p1','-d',str(root/'source')],input=text.encode(),check=True)
 subprocess.run(['patch','--batch','--silent','--fuzz=0','-p1','-d',str(root/'source')],input=(repo/'xnu/patches/0323-river-fit-quiet-tone-within-mute-deadline.patch').read_bytes(),check=True)
 sections=['diff --git '+x for x in (repo/'xnu/patches/0325-river-mute-immediately-after-rendered-eos.patch').read_text().split('diff --git ')[1:]]
 text=''.join(x for x in sections if x.splitlines()[0].split()[2]=='a/bsd/dev/river_adsp_stream.h')
 subprocess.run(['patch','--batch','--silent','--fuzz=0','-p1','-d',str(root/'source')],input=text.encode(),check=True)
 (root/'route-actual.h').write_text(target.read_text())
 (root/'stream-actual.h').write_text(stream_target.read_text().split('static int\nriver_adsp_stream_sysctl',1)[0].replace('__asm__ volatile("dmb sy" ::: "memory");','host_barrier();'))
 binary=root/'route'
 subprocess.run([os.environ.get('CC','cc'),'-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(root),str(repo/'tools/tests/river-adsp-stream-host.c'),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
