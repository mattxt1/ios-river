#!/usr/bin/env python3
"""Check the actual compatibility syscall against the observed iOS16 layout."""
from pathlib import Path
import tempfile,subprocess,os
repo=Path(__file__).resolve().parent.parent
p=next((repo/'xnu/patches').glob('0212-*')).read_text()
# Reconstruct only the complete syscall from its baseline and the two changes.
base=next((repo/'xnu/patches').glob('0050-*')).read_text()
s='\n'.join(x[1:] for x in base.splitlines() if x.startswith('+') and not x.startswith('+++'))+'\n'
s=s[s.index('static uint64_t sweet_sandbox_seen;'):s.index('static const struct mac_policy_ops')]
with tempfile.TemporaryDirectory(prefix='river-token-abi-',dir=repo.parents[1]/'work') as d:
 root=Path(d);path=root/'bsd/kern/sweet_amfi.c';path.parent.mkdir(parents=True)
 # Apply the actual syscall substitutions; context from unrelated AMFI lines
 # is not required for this isolated syscall model.
 for patch in [next((repo/'xnu/patches').glob('0212-*')),next((repo/'xnu/patches').glob('0338-*'))]:
  lines=patch.read_text().splitlines();old=[];new=[];inside=False
  for line in lines:
   if line.startswith('@@'):
    if old:
     a='\n'.join(old)+'\n';b='\n'.join(new)+'\n'
     if a in s:s=s.replace(a,b,1)
    old=[];new=[];inside=True;continue
   if not inside:continue
   if line.startswith(' '):old.append(line[1:]);new.append(line[1:])
   elif line.startswith('-'):old.append(line[1:])
   elif line.startswith('+'):new.append(line[1:])
  if old:
   a='\n'.join(old)+'\n';b='\n'.join(new)+'\n'
   if a in s:s=s.replace(a,b,1)
 # First0212 hunk includes a leading declaration with unrelated comments;
 # ensure the observed syscall itself actually contains the new cases.
 if 'case 5: {' not in s:raise RuntimeError('actual syscall reconstruction failed')
 s=s.replace('__unused user_addr_t arg','user_addr_t arg')
 (root/'token-actual.h').write_text(s)
 subprocess.run([os.environ.get('CC','cc'),'-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',d,str(repo/'tools/tests/river-access-token-abi-host.c'),'-o',str(root/'test')],check=True)
 subprocess.run([str(root/'test')],check=True)
