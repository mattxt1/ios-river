#!/usr/bin/env python3
"""Exercise actual queued READ publication/dispatch/order/cache/timestamp code."""
from pathlib import Path
import subprocess,tempfile,os
repo=Path(__file__).resolve().parent.parent;patches=repo/'xnu/patches'
def added(pattern,name):
 section=next(x for x in next(patches.glob(pattern)).read_text().split('diff --git ')[1:] if x.startswith('a/bsd/dev/'+name+' '))
 return '\n'.join(x[1:] for x in section.splitlines() if x.startswith('+') and not x.startswith('+++'))+'\n'
with tempfile.TemporaryDirectory(prefix='river-mic-pipeline-',dir=repo.parents[1]/'work') as d:
 root=Path(d);source=root/'source';(source/'bsd/dev').mkdir(parents=True)
 names=['river_adsp_capture.h','river_adsp_pcm.h'];(source/'bsd/dev/river_adsp_capture.h').write_text(added('0331-*',names[0]));(source/'bsd/dev/river_adsp_pcm.h').write_text(added('0303-*',names[1]))
 for pattern in ['0304-*','0321-*','0332-*','0333-*','0334-*','0335-*','0336-*','0337-*']:
  sections=['diff --git '+x for x in next(patches.glob(pattern)).read_text().split('diff --git ')[1:]]
  text=''.join(x for x in sections if x.splitlines()[0].split()[2][2:] in ['bsd/dev/'+n for n in names])
  if text:subprocess.run(['patch','--batch','--silent','--fuzz=0','-p1','-d',str(source)],input=text.encode(),check=True)
 cap=(source/'bsd/dev/river_adsp_capture.h').read_text();state=cap[cap.index('static struct {'):cap.index('static int\nri_mic_command')];done=cap[cap.index('static bool\nri_mic_read_done'):cap.index('static void\nri_mic_invalidate')]
 pcm=(source/'bsd/dev/river_adsp_pcm.h').read_text();decode=pcm[pcm.index('static int\nri_pcm_decode'):pcm.index('static int\nri_pcm_receive')]
 pipe=added('0337-*','river_mic_pipeline.h');a=pipe.index('static void\nri_mic_invalidate_slot');b=pipe.index('static int\nri_mic_read_pipeline',a);pipe=pipe[:a]+'static void ri_mic_invalidate_slot(vm_offset_t);\n'+pipe[b:];pipe=pipe.replace('__asm__ volatile("dmb sy" ::: "memory");','host_barrier();')
 (root/'mic-state-actual.h').write_text(state);(root/'mic-done-actual.h').write_text(done);(root/'mic-decode-actual.h').write_text(decode);(root/'mic-pipeline-actual.h').write_text(pipe)
 subprocess.run([os.environ.get('CC','cc'),'-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=address,undefined','-I',d,str(repo/'tools/tests/river-mic-pipeline-host.c'),'-o',str(root/'test')],check=True)
 subprocess.run([str(root/'test')],check=True)
