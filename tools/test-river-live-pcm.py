#!/usr/bin/env python3
"""Compile committed live queue/pipeline and APR decoder with fault injection."""
from pathlib import Path
import subprocess,os,tempfile
repo=Path(__file__).resolve().parent.parent
patches=repo/'xnu/patches'
def added(patch,name):
 section=(patches/patch).read_text().split('diff --git a/bsd/dev/'+name,1)[1].split('diff --git ',1)[0]
 return '\n'.join(l[1:] for l in section.splitlines() if l.startswith('+') and not l.startswith('+++'))+'\n'
with tempfile.TemporaryDirectory(prefix='river-live-',dir=repo.parents[1]/'work') as d:
 root=Path(d)
 queue=added('0327-river-live-pcm-queue-and-dsp-pipeline.patch','river_audio_queue.h').split('static int\nriver_audio_queue_sysctl')[0].replace('static int ri_audio_queue_launch(void);','')
 pipeline=added('0327-river-live-pcm-queue-and-dsp-pipeline.patch','river_adsp_pipeline.h').replace('__asm__ volatile("dmb sy" ::: "memory");','host_barrier();')
 pcm=added('0303-river-pcm-map-asm-afe-prerequisites.patch','river_adsp_pcm.h')
 # Reconstruct decoder including the exact current EOS change.
 target=root/'source/bsd/dev/river_adsp_pcm.h';target.parent.mkdir(parents=True);target.write_text(pcm)
 for pattern in ['0304-*','0321-*']:
  patch=next(patches.glob(pattern)).read_text()
  sections=['diff --git '+x for x in patch.split('diff --git ')[1:]]
  text=''.join(x for x in sections if x.startswith('diff --git a/bsd/dev/river_adsp_pcm.h '))
  subprocess.run(['patch','--batch','--silent','--fuzz=0','-p1','-d',str(root/'source')],input=text.encode(),check=True)
 pcm=target.read_text();types=pcm[pcm.index('struct ri_apr_request'):pcm.index('static struct {')]
 decoder=pcm[pcm.index('static int\nri_pcm_decode'):pcm.index('static int\nri_pcm_receive')]
 for name,s in [('queue-actual.h',queue),('pipeline-actual.h',pipeline),('decode-actual.h',types+decoder)]: (root/name).write_text(s)
 binary=root/'test'
 subprocess.run([os.environ.get('CC','cc'),'-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-pthread','-I',d,str(repo/'tools/tests/river-live-pcm-host.c'),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
