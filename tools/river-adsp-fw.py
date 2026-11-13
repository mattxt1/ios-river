#!/usr/bin/env python3
"""Validate River's private stock ADSP split firmware; optionally stage offline.
No device writes, secure calls or signature-authentication claims.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

BASE, SIZE = 0x8d600000, 0x1100000
HASH, RELOC = 2 << 24, 1 << 27

def validate(root):
    root = Path(root)
    mdt = (root / 'adsp.mdt').read_bytes()
    if len(mdt) < 52 or mdt[:7] != b'\x7fELF\x01\x01\x01':
        raise ValueError('expected ELF32 little-endian version1 ADSP MDT')
    kind, machine, version = struct.unpack_from('<HHI', mdt, 16)
    if (kind, machine, version) != (2, 164, 1):
        raise ValueError('expected Hexagon executable')
    phoff = struct.unpack_from('<I', mdt, 28)[0]
    ehsize, entsize, count = struct.unpack_from('<HHH', mdt, 40)
    if ehsize != 52 or entsize != 32 or not 1 <= count <= 32 or phoff < 52 or phoff + count * 32 > len(mdt):
        raise ValueError('invalid ELF program-header table')
    headers = [struct.unpack_from('<8I', mdt, phoff + i * 32) for i in range(count)]
    hashes = [i for i, h in enumerate(headers) if h[6] & (7 << 24) == HASH]
    if len(hashes) != 1 or hashes[0] == 0:
        raise ValueError('expected one non-header hash segment')
    hash_index = hashes[0]
    header_size = headers[0][4]
    if headers[0][0] != 0 or header_size < phoff + count * 32:
        raise ValueError('invalid header segment')
    parts = {}
    for i, h in enumerate(headers):
        if h[4]:
            data = (root / f'adsp.b{i:02d}').read_bytes()
            if len(data) != h[4]:
                raise ValueError(f'adsp.b{i:02d}: file-size mismatch')
            parts[i] = data
    hash_data = parts[hash_index]
    if len(hash_data) < 40 + count * 32:
        raise ValueError('truncated embedded hash table')
    if len(mdt) != header_size + len(hash_data) or parts[0] != mdt[:header_size] or mdt[header_size:] != hash_data:
        raise ValueError('MDT differs from split header/hash segments')
    for i, data in parts.items():
        if i != hash_index and hashlib.sha256(data).digest() != hash_data[40 + i * 32:72 + i * 32]:
            raise ValueError(f'adsp.b{i:02d}: embedded SHA256 mismatch')
    segments = []
    for i, h in enumerate(headers):
        if i == hash_index or h[0] != 1 or not h[5]:
            continue
        _, _, _, addr, filesz, memsz, flags, _ = h
        if not flags & RELOC or filesz > memsz or addr + memsz > 0x100000000:
            raise ValueError(f'segment {i}: invalid relocatable range')
        segments.append((i, addr, filesz, memsz))
    if not segments:
        raise ValueError('no loadable segments')
    lo = min(s[1] for s in segments)
    hi = max(s[1] + s[3] for s in segments)
    extent = (hi - lo + 4095) & ~4095
    if lo & 4095 or extent != SIZE:
        raise ValueError('ADSP extent does not match River reserved carveout')
    occupied = []
    records = []
    for i, addr, filesz, memsz in segments:
        start, end = addr - lo, addr - lo + memsz
        if start < 0 or end > SIZE or any(start < b and a < end for a, b in occupied):
            raise ValueError(f'segment {i}: overlaps/exceeds carveout')
        occupied.append((start, end))
        records.append(dict(index=i, physical=BASE + start, file_size=filesz, memory_size=memsz))
    manifest = dict(pas_id=1, carveout_base=BASE, carveout_size=SIZE,
                    original_base=lo, original_end=hi, relocation=BASE-lo,
                    metadata_sha256=hashlib.sha256(mdt).hexdigest(),
                    embedded_file_hashes_verified=True, signature_authenticated=False,
                    device_written=False, segments=records)
    return manifest, mdt, parts

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('firmware', type=Path)
    p.add_argument('--stage', type=Path)
    a = p.parse_args()
    manifest, metadata, parts = validate(a.firmware)
    if a.stage:
        a.stage.mkdir(mode=0o700, parents=True, exist_ok=False)
        image = bytearray(SIZE)
        for s in manifest['segments']:
            start = s['physical'] - BASE
            if s['file_size']:
                image[start:start+s['file_size']] = parts[s['index']]
        for name, data in [('adsp-pas-metadata.bin', metadata), ('adsp-carveout.bin', image)]:
            path = a.stage / name
            with path.open('xb') as f:
                f.write(data)
            path.chmod(0o600)
        manifest['image_sha256'] = hashlib.sha256(image).hexdigest()
        (a.stage / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(manifest, indent=2))

if __name__ == '__main__':
    main()
