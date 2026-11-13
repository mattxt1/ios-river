#!/usr/bin/env python3
"""Compile original0267 and actual0290 battery paths with fault injection.

No phone access. Uses patch content, not a duplicated estimator/decision model.
"""
from pathlib import Path
import argparse
import re
import subprocess
import tempfile

root=Path(__file__).resolve().parent.parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--base-patch',type=Path,default=root/'xnu/patches/0267-river-battery-jg30-power-source.patch',help='optional hydrated copy of the exact0267 patch')
args=parser.parse_args()
old=args.base_patch.read_text()
fix=(root/'xnu/patches/0290-river-battery-valid-samples.patch').read_text()
readonly_fix=(root/'xnu/patches/0291-river-battery-readonly-sampling.patch').read_text()
def section(patch,path):
    marker='diff --git a/'+path+' b/'+path
    return patch.split(marker,1)[1].split('diff --git ',1)[0]
def additions(part):
    return '\n'.join(line[1:] for line in part.splitlines() if line.startswith('+') and not line.startswith('+++'))+'\n'
def function(text,name):
    start=text.index(name)
    brace=text.index('{',start)
    depth=1;i=brace+1
    while depth:
        if text[i]=='{':depth+=1
        elif text[i]=='}':depth-=1
        i+=1
    # Retain the exact return type on the preceding line.
    return text[text.rfind('\n',0,start-1)+1:i]+'\n'
def apply_matching_hunks(text,part):
    applied=0
    for hunk in re.split(r'^@@[^\n]*\n',part,flags=re.M)[1:]:
        before=[];after=[]
        for line in hunk.splitlines(keepends=True):
            if line.startswith((' ','-')):before.append(line[1:])
            if line.startswith((' ','+')):after.append(line[1:])
        a=''.join(before);b=''.join(after)
        if a and a in text:
            if text.count(a)!=1:raise AssertionError('ambiguous patch context')
            text=text.replace(a,b,1);applied+=1
    return text,applied
keys=additions(section(old,'bsd/dev/sweet_keys.c'))
diagnostic=function(keys,'river_battery_sysctl SYSCTL_HANDLER_ARGS')
# The added LUT/functions contain all of the exact sample implementation.
keys=keys.split('static int\nriver_battery_sysctl',1)[0]
poll=function(additions(section(old,'iokit/Kernel/arm/SweetBattery.cpp')),'SweetBattery::pollRiver(void)')
newkeys,n=apply_matching_hunks(keys,section(fix,'bsd/dev/sweet_keys.c'));assert n==1
newpoll,n=apply_matching_hunks(poll,section(fix,'iokit/Kernel/arm/SweetBattery.cpp'));assert n==2
readonly_keys,n=apply_matching_hunks(newkeys,section(readonly_fix,'bsd/dev/sweet_keys.c'));assert n==3
assert '+\tbool fRiverHasSample;' in fix and '+\tfRiverHasSample = false;' in fix
with tempfile.TemporaryDirectory(prefix='river-battery-test-') as tmp:
    tmp=Path(tmp)
    for baseline,readonly,a,b in [(True,False,keys,poll),(False,False,newkeys,newpoll),(False,True,readonly_keys,newpoll)]:
        (tmp/'battery-sample-added.h').write_text(a)
        (tmp/'battery-poll-added.h').write_text(b)
        (tmp/'battery-diagnostic-added.h').write_text(diagnostic)
        binary=tmp/('baseline' if baseline else ('readonly' if readonly else 'fixed'))
        subprocess.run(['c++','-std=c++17','-O2','-Wall','-Wextra','-Werror',
                        '-DBUG_BASELINE='+str(int(baseline)),'-DREADONLY_SAMPLE='+str(int(readonly)),'-I',str(tmp),
                        str(root/'tools/tests/river-battery-host.cpp'),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
