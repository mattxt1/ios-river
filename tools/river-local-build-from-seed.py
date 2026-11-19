#!/usr/bin/env python3
"""Offline continuation in a NEW copy of a known local River build job.

Invoked by river-local-build.sh after it archives committed patch inputs.
The seed is never edited. This is an incremental build, not a fresh upstream
source reproduction. Its manifest records the seed and exact patch prefix.
"""
from pathlib import Path
import hashlib, json, os, re, shutil, subprocess, sys

seed, job = (Path(p).resolve() for p in sys.argv[1:3])
revision = sys.argv[3]
if not seed.is_dir() or seed == job or seed in job.parents or job in seed.parents:
    raise SystemExit('seed must be a separate existing job directory')
if (job/'dxb').exists():
    raise SystemExit('destination already contains a build tree')
def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
def patches(path):
    return sorted(p for p in path.iterdir() if re.match(r'^\d{4}-.+\.patch$', p.name))
before = patches(seed/'input/xnu/patches')
after = patches(job/'input/xnu/patches')
before_data={p.name:p.read_bytes() for p in before}
provenance=None
# Historical incremental jobs did not always copy each intermediate patch to
# input/. An explicit build manifest can establish the complete committed
# prefix, but only when its exact kernel and ancestor revision still verify.
if os.environ.get('RIVER_LOCAL_BUILD_SEED_MANIFEST'):
    provenance=Path(os.environ['RIVER_LOCAL_BUILD_SEED_MANIFEST']).resolve()
    record=json.loads(provenance.read_text())
    old_revision=record['source_commit']
    seed_kernel=seed/'dxb/fakeroot/System/Library/Kernels/kernel.development.sweet'
    if record.get('exit')!=0 or Path(record['kernel']).resolve()!=seed_kernel.resolve() or digest(seed_kernel)!=record['kernel_sha256']:
        raise SystemExit('seed manifest/kernel verification failed')
    repo=Path(__file__).resolve().parent.parent
    subprocess.run(['git','merge-base','--is-ancestor',old_revision,revision],cwd=repo,check=True)
    names=subprocess.check_output(['git','ls-tree','-r','--name-only',old_revision,'xnu/patches'],cwd=repo,text=True).splitlines()
    complete={Path(n).name:subprocess.check_output(['git','show',old_revision+':'+n],cwd=repo) for n in names if re.match(r'^\d{4}-.+\.patch$',Path(n).name)}
    if any(name not in complete or data!=complete[name] for name,data in before_data.items()):
        raise SystemExit('stored seed inputs conflict with manifest revision')
    before_data=complete
before_names=sorted(before_data)
if not before_names or len(before_names) >= len(after) or before_names != [p.name for p in after[:len(before_names)]]:
    raise SystemExit('seed must contain an exact, shorter prefix of committed numbered patches')
for new in after[:len(before_names)]:
    if before_data[new.name] != new.read_bytes():
        raise SystemExit('seed patch differs from committed input: '+new.name)
if (seed/'dxb').is_symlink():
    raise SystemExit('seed dxb must be a real directory, not a symlink')
runner = seed/'dxb/resume-kernel.sh'
if not runner.is_file() or not runner.read_text().rstrip().endswith('build_xnu'):
    raise SystemExit('seed lacks its explicit kernel-only resume script')
print('Copying local seed into isolated job:', job, flush=True)
# APFS clones avoid duplicating large, unchanged object files on this Mac.
# Preserve symlinks; no writes are ever made through the seed's directory.
subprocess.run(['cp','-cR',str(seed/'dxb'),str(job/'dxb')],check=True)
build = job/'dxb'
changed=set()
for patch in after[len(before_names):]:
    data=patch.read_bytes()
    subprocess.run(['patch','--batch','--fuzz=0','--dry-run','-p1','-d',str(build/'xnu')],input=data,check=True)
    subprocess.run(['patch','--batch','--fuzz=0','-p1','-d',str(build/'xnu')],input=data,check=True)
    for path in re.findall(rb'^\+\+\+ b/(.+)$',data,re.M):
        changed.add(path.decode())
    shutil.copyfile(patch,build/'patches'/patch.name)
for path in changed:
    # Force changed translation units to compile even when copied dependency
    # files still refer to an older absolute source directory.
    if Path(path).suffix not in ('.c','.cpp'):
        continue
    objects=list((build/'build/xnu.obj').rglob(Path(path).stem+'.o'))
    for obj in objects:
        obj.unlink()
for name in ('clang','clang++'):
    compiler=subprocess.check_output(['xcrun','-find',name],text=True).strip()
    wrapper=job/'ccache-bin'/name
    wrapper.write_text('#!/bin/sh\nexec '+shutil.which('ccache')+' '+compiler+' "$@"\n')
    wrapper.chmod(0o755)
(job/'bin/xcodes').write_text('#!/bin/sh\nexit 0\n')
(job/'bin/xcodes').chmod(0o755)
env=os.environ.copy()
env.update(CCACHE_WRAP=str(job/'ccache-bin'),CCACHE_DIR=str(job.parent/'ccache'),CCACHE_BASEDIR=str(build),MACOS_VERSION='13.5',KERNEL_CONFIG='DEVELOPMENT',ARCH_CONFIG='ARM64',MACHINE_CONFIG='SWEET',SWEET_BOARD='river')
env['PATH']=str(job/'bin')+':'+env['PATH']
log=job/'build.log'
print('Building committed source',revision,'log:',log,flush=True)
with log.open('wb') as output:
    result=subprocess.run(['bash','resume-kernel.sh'],cwd=build,env=env,stdout=output,stderr=subprocess.STDOUT)
manifest={
    'source_commit':revision,'seed':str(seed),'incremental_seed_copy':True,
    'seed_manifest':str(provenance) if provenance else None,
    'seed_prefix':{n:hashlib.sha256(before_data[n]).hexdigest() for n in before_names},
    'new_patches':{p.name:digest(p) for p in after[len(before_names):]},
    'changed_source_sha256':{p:digest(build/'xnu'/p) for p in sorted(changed)},
    'build_log':str(log),'exit':result.returncode,
}
kernel=build/'fakeroot/System/Library/Kernels/kernel.development.sweet'
if result.returncode==0 and kernel.is_file():
    manifest.update(kernel=str(kernel),kernel_sha256=digest(kernel))
else:
    result.returncode=result.returncode or 1
(job/'build-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
if result.returncode:
    raise SystemExit('Native build failed; inspect '+str(log))
print('Kernel:',kernel,'\nSHA256:',manifest['kernel_sha256'],flush=True)
