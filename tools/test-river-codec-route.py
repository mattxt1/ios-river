#!/usr/bin/env python3
"""Model GPIO SPI, FLL/AIF clocks and muted SLIMRX routing using actual code."""
from pathlib import Path
import subprocess,tempfile,os,argparse
repo=Path(__file__).resolve().parent.parent
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--kernel-source',type=Path);args=parser.parse_args()
with tempfile.TemporaryDirectory(prefix='river-codec-path-') as d:
 root=Path(d);source=root/'source';(source/'bsd/dev').mkdir(parents=True)
 def apply_sections(p,allowed):
  sections=['diff --git '+x for x in p.read_text().split('diff --git ')[1:]]
  text=''.join(x for x in sections if x.splitlines()[0].split()[2][2:] in allowed)
  if text:subprocess.run(['patch','--batch','--silent','--fuzz=0','-p1','-d',str(source)],input=text.encode(),check=True)
 headers={'bsd/dev/river_codec_probe.h','bsd/dev/river_codec_clock.h','bsd/dev/river_codec_i2s.h'}
 for number in range(274,282):
  p=list((repo/'xnu/patches').glob(f'{number:04d}-*.patch'));assert len(p)==1;apply_sections(p[0],headers)
 if args.kernel_source:
  for name in headers:assert (source/name).read_bytes()==(args.kernel_source/name).read_bytes(),name+' baseline differs'
 apply_sections(repo/'xnu/patches/0315-river-muted-slimrx-aif-codec-route.patch',headers|{'bsd/dev/river_adsp_codec_route.h'})
 apply_sections(repo/'xnu/patches/0316-river-codec-mixer-live-status.patch',headers|{'bsd/dev/river_adsp_codec_route.h'})
 apply_sections(repo/'xnu/patches/0317-river-muted-pcm-through-active-codec-route.patch',{'bsd/dev/river_adsp_codec_route.h'})
 sections=['diff --git '+x for x in (repo/'xnu/patches/0318-river-prepare-afe-before-codec-channel.patch').read_text().split('diff --git ')[1:]]
 text=''.join(x for x in sections if x.splitlines()[0].split()[2] in ['a/bsd/dev/river_adsp_codec_route.h'])
 subprocess.run(['patch','--batch','--silent','--fuzz=0','-p1','-d',str(source)],input=text.encode(),check=True)
 sections=['diff --git '+x for x in (repo/'xnu/patches/0319-river-ngd-post-afe-preflight-readbacks.patch').read_text().split('diff --git ')[1:]]
 text=''.join(x for x in sections if x.splitlines()[0].split()[2]=='a/bsd/dev/river_adsp_codec_route.h')
 subprocess.run(['patch','--batch','--silent','--fuzz=0','-p1','-d',str(source)],input=text.encode(),check=True)
 probe=(source/'bsd/dev/river_codec_probe.h').read_text().split('static int\nriver_codec_probe_sysctl',1)[0]
 clock=(source/'bsd/dev/river_codec_clock.h').read_text().split('static int\nriver_codec_clock_sysctl',1)[0]
 route=(source/'bsd/dev/river_adsp_codec_route.h').read_text().split('static int\nriver_adsp_codec_path_sysctl',1)[0]
 for name,s in [('codec-probe-added.h',probe),('codec-clock-added.h',clock),('codec-route-added.h',route),('river_codec_i2s.h',(source/'bsd/dev/river_codec_i2s.h').read_text())]:(root/name).write_text(s)
 p=root/'test';subprocess.run([os.environ.get('CC','cc'),'-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=address,undefined','-I',d,str(repo/'tools/tests/river-codec-route-host.c'),'-o',str(p)],check=True);subprocess.run([str(p)],check=True)
