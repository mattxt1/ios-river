#!/usr/bin/env python3
"""Exercise actual ADSP probe patch and malformed firmware without a phone."""
import hashlib
import importlib.util
from pathlib import Path
import struct
import subprocess
import tempfile
r=Path(__file__).resolve().parent.parent
spec=importlib.util.spec_from_file_location('fw',r/'tools/river-adsp-fw.py');fw=importlib.util.module_from_spec(spec);spec.loader.exec_module(fw)
with tempfile.TemporaryDirectory() as d:
    root=Path(d)
    patch=(r/'xnu/patches/0282-river-adsp-prerequisites.patch').read_text().split('diff --git a/bsd/dev/river_adsp_probe.h',1)[1]
    code='\n'.join(x[1:] for x in patch.splitlines() if x.startswith('+') and not x.startswith('+++'))
    dev=root/'bsd/dev';dev.mkdir(parents=True)
    (dev/'river_adsp_probe.h').write_text(code+'\n')
    subprocess.run(['git','apply',str(r/'xnu/patches/0283-river-adsp-metadata-auth.patch')],cwd=root,check=True)
    subprocess.run(['git','apply',str(r/'xnu/patches/0284-river-adsp-builtin-sha256.patch')],cwd=root,check=True)
    code=(dev/'river_adsp_probe.h').read_text()
    code='\n'.join(x for x in code.splitlines() if not x.startswith('#include <'))
    code=code.split('#include "river_adsp_metadata.h"',1)[0]
    (root/'river-adsp-added.h').write_text(code)
    binary=root/'probe'
    subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-I',d,str(r/'tools/tests/river-adsp-probe-host.c'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
    metadata=(dev/'river_adsp_metadata.h').read_text()
    metadata=metadata.split('static int\nriver_adsp_metadata_sysctl',1)[0]
    # The private stock MDT is deliberately absent from the source tree.
    # Substitute only its expected digest with a reproducible fixture digest.
    import re
    digest=hashlib.sha256(bytes(i*7&255 for i in range(7516))).digest()
    metadata=re.sub(r'(ri_adsp_expected_sha256\[32\]=\{).*?(\};)',
                    lambda m:m[1]+','.join(hex(x) for x in digest)+m[2],metadata,flags=re.S)
    (root/'river-adsp-metadata-added.h').write_text(metadata)
    binary=root/'metadata'
    subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function',
                    '-Wno-deprecated-declarations','-I',d,str(r/'tools/tests/river-adsp-metadata-host.c'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
    def fixture(overlap=False,extent=None):
        head=bytearray(180);head[:7]=b'\x7fELF\x01\x01\x01'
        struct.pack_into('<HHI',head,16,2,164,1)
        struct.pack_into('<I',head,28,52)
        struct.pack_into('<HHH',head,40,52,32,4)
        hs=[(0,0,0,0,180,0,0x7000000,0),(0,0,0,0,168,4096,fw.HASH,4096),
            (1,0,0,0x8a200000,4,4096,fw.RELOC|5,4096),
            (1,0,0,0x8a200000 if overlap else 0x8a201000,0,extent or (fw.SIZE if overlap else fw.SIZE-4096),fw.RELOC|5,4096)]
        for i,h in enumerate(hs):struct.pack_into('<8I',head,52+i*32,*h)
        hashes=bytearray(168)
        for i,p in [(0,head),(2,b'ABCD')]:hashes[40+i*32:72+i*32]=hashlib.sha256(p).digest()
        for name,data in [('adsp.mdt',head+hashes),('adsp.b00',head),('adsp.b01',hashes),('adsp.b02',b'ABCD')]:
            (root/name).write_bytes(data)
    fixture();manifest,_,_=fw.validate(root)
    assert manifest['carveout_size']==fw.SIZE and not manifest['signature_authenticated']
    def rejected():
        try:fw.validate(root)
        except (ValueError,FileNotFoundError):return
        raise AssertionError('malformed firmware accepted')
    (root/'adsp.b02').write_bytes(b'ABCE');rejected()
    fixture();(root/'adsp.b02').write_bytes(b'A');rejected()
    fixture(overlap=True);rejected()
    fixture(extent=fw.SIZE);rejected()
    fixture();mdt=bytearray((root/'adsp.mdt').read_bytes());mdt[18]=0;(root/'adsp.mdt').write_bytes(mdt);rejected()
    print('PASS: ADSP firmware hashes, metadata, architecture, overlapping ranges and exact carveout bounds')
# Exercise the actual repacker's container update without importing its toolchain.
import ast
source=ast.parse((r/'tools/river-repack-audio.py').read_text())
ns={'struct':struct}
exec(compile(ast.Module(body=[n for n in source.body if isinstance(n,ast.FunctionDef) and n.name in ('require','replace_helper')],type_ignores=[]),'<actual-repacker>','exec'),ns)
def container(entries):
    offset=(12+128*len(entries)+15)&~15
    table=bytearray();data=bytearray()
    for name,mode,content,reserved in entries:
        table+=struct.pack('<112sIIII',name,mode,offset+len(data),len(content),reserved)
        data+=content+b'\0'*(-len(content)&15)
    return (b'RIVFILES'+struct.pack('<I',len(entries))+table).ljust(offset,b'\0')+data
def contents(blob):
    result={}
    for i in range(struct.unpack_from('<I',blob,8)[0]):
        name,mode,off,size,reserved=struct.unpack_from('<112sIIII',blob,12+128*i)
        result[name.split(b'\0')[0]]=(mode,blob[off:off+size],reserved)
    return result
clock=b'/usr/local/bin/river-codec-clock';adsp=b'/usr/local/bin/river-adsp-status'
original=[(clock,0o755,b'oldclock',0),(b'/untouched',0o640,b'private unchanged payload',7)]
out=ns['replace_helper'](container(original),b'newclock',b'adsphelper')
data=contents(out)
assert data[b'/untouched']==(0o640,b'private unchanged payload',7)
assert data[clock]==(0o755,b'newclock',0) and data[adsp]==(0o755,b'adsphelper',0)
out=ns['replace_helper'](out,b'nextclock',b'nextadsp')
assert len(contents(out))==3 and contents(out)[adsp][1]==b'nextadsp'
try:ns['replace_helper'](container(original+[(adsp,0o755,b'a',0),(adsp,0o755,b'b',0)]),b'c',b'd')
except ValueError:pass
else:raise AssertionError('duplicate ADSP helper accepted')
print('PASS: repacker adds/replaces ADSP helper, preserves unrelated entries and rejects duplicates')
