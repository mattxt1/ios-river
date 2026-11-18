"""Reconstruct selected actual sources from the committed patch series."""
from pathlib import Path
import subprocess,tempfile
REPO=Path(__file__).resolve().parent.parent
def source(name):
 path='bsd/dev/'+name
 with tempfile.TemporaryDirectory(prefix='river-source-',dir=REPO.parents[1]/'work') as d:
  root=Path(d);(root/'bsd/dev').mkdir(parents=True)
  for p in sorted((REPO/'xnu/patches').glob('*.patch')):
   data=p.read_bytes()
   if ('diff --git a/'+path+' ').encode() not in data:continue
   parts=['diff --git '+s for s in data.decode().split('diff --git ')[1:]]
   part=''.join(s for s in parts if s.startswith('diff --git a/'+path+' '))
   if part:subprocess.run(['patch','--batch','--silent','--fuzz=0','-p1','-d',d],input=part.encode(),check=True)
  return (root/path).read_text()
