#!/usr/bin/env python3
"""Compile actual AFE/ADM route state/cleanup code and speaker mute policy."""
from pathlib import Path
import subprocess,tempfile,os,argparse
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("--kernel-source",type=Path,help="Optional applied XNU tree through0303 to also verify complete patch application and actual volume helper")
args=parser.parse_args()
repo=Path(__file__).resolve().parent.parent
patch=(repo/'xnu/patches/0304-river-afe-config-adm-matrix-routing.patch').read_text()
p=patch.split('diff --git a/bsd/dev/river_adsp_route.h',1)[1].split('diff --git ',1)[0]
source='\n'.join(x[1:] for x in p.splitlines() if x.startswith('+') and not x.startswith('+++'))+'\n'
source=source.split('static int\nriver_adsp_route_sysctl',1)[0]
with tempfile.TemporaryDirectory(prefix='river-adsp-route-') as directory:
 root=Path(directory);(root/'route-actual.h').write_text(source)
 binary=root/'route'
 subprocess.run([os.environ.get('CC','cc'),'-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(root),str(repo/'tools/tests/river-adsp-route-host.c'),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)

if args.kernel_source:
 with tempfile.TemporaryDirectory(prefix='river-route-apply-') as directory:
  root=Path(directory)
  for name in ['bsd/dev/sweet_keys.c','bsd/dev/river_adsp_pcm.h','bsd/dev/river_adsp_probe.h']:
   p=root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes((args.kernel_source/name).read_bytes())
  subprocess.run(['patch','--batch','--silent','-p1','-d',str(root)],input=(repo/'xnu/patches/0304-river-afe-config-adm-matrix-routing.patch').read_bytes(),check=True)
  s=(root/'bsd/dev/sweet_keys.c').read_text();start=s.index('static void\nriver_speaker_set_volume_locked(');end=s.index('\nvoid\nriver_speaker_set_volume(',start)
  (root/'volume-actual.h').write_text(s[start:end])
  harness=(repo/'tools/tests/river-speaker-volume-host.c').read_text().replace('static bool ri_speaker_ok;','static bool ri_speaker_ok,ri_afe_mute_hold;')
  harness=harness.replace('int main(void)\n{','''int main(void)
 {
  reset();ri_afe_mute_hold=true;river_speaker_set_volume_locked(80,false);
  assert((control&0x10) && (digital&0x3ff8)==0x2678);ri_afe_mute_hold=false;
 ''')
  (root/'volume-host.c').write_text(harness)
  binary=root/'volume';subprocess.run(['cc','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(root),str(root/'volume-host.c'),'-o',str(binary)],check=True);subprocess.run([str(binary)],check=True)
  print('0304 patch dry-run/application and actual speaker mute policy passed')
