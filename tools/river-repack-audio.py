#!/usr/bin/env python3
"""Repack a verified temporary River image with a new kernel and clock helper.

Preserves the prior shim, command line, ADT configuration, firmware and all
other RiverFiles entries. Never flashes or accesses the phone. Private inputs
and output images must remain outside Git.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import tempfile

import adt
import mkpayload

NAMES = ('RiverGPUFW', 'RiverWCNSSMeta', 'RiverWCNSSImage', 'RiverWCNSSNV', 'RiverFiles')


def digest(data):
    return hashlib.sha256(data).hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def replace_helper(blob, replacement):
    require(blob[:8] == b'RIVFILES', 'Invalid RiverFiles magic')
    count, = struct.unpack_from('<I', blob, 8)
    require(count <= 64 and 12 + count * 128 <= len(blob), 'Invalid file table')
    entries = []
    found = 0
    for i in range(count):
        name, mode, off, size, reserved = struct.unpack_from('<112sIIII', blob, 12 + 128 * i)
        require(off + size <= len(blob), 'File outside container')
        data = blob[off:off + size]
        if name.split(b'\0')[0] == b'/usr/local/bin/river-codec-clock':
            found += 1
            data = replacement
        entries.append((name, mode, data, reserved))
    require(found == 1, 'Expected exactly one existing clock helper')
    offset = (12 + count * 128 + 15) & ~15
    table, data = bytearray(), bytearray()
    for name, mode, content, reserved in entries:
        table += struct.pack('<112sIIII', name, mode, offset + len(data), len(content), reserved)
        data += content + b'\0' * (-len(content) & 15)
    return (b'RIVFILES' + struct.pack('<I', count) + table).ljust(offset, b'\0') + data


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--base', type=Path, required=True)
    ap.add_argument('--base-sha256', required=True)
    ap.add_argument('--kernel', type=Path, required=True)
    ap.add_argument('--clock-helper', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    require(not args.output.exists(), 'Refusing to overwrite output')
    base = args.base.read_bytes()
    require(digest(base) == args.base_sha256.lower(), 'Base image hash mismatch')
    require(base[:8] == b'ANDROID!', 'Expected Android boot image')
    ks, = struct.unpack_from('<I', base, 8)
    rs, = struct.unpack_from('<I', base, 16)
    page, = struct.unpack_from('<I', base, 36)
    require(page in (2048, 4096, 8192, 16384), 'Unsupported page size')
    ro = page + (ks + page - 1) // page * page
    require(ro + rs <= len(base), 'Truncated boot image')
    prior = base[ro:ro + rs]
    require(prior[:8] == b'SWEETXNU', 'Expected SWEETXNU payload')
    count, = struct.unpack_from('<I', prior, 12)
    require(count <= 64 and 0x2a8 + count * 32 <= len(prior), 'Invalid segment table')
    segments = [struct.unpack_from('<4Q', prior, 0x2a8 + i * 32) for i in range(count)]
    for va, vs, off, size in segments:
        require(size <= vs and off + size <= len(prior), 'Invalid payload segment')
    cmdline = prior[0x48:0x2a8].split(b'\0')[0]
    require(len(cmdline) < 608 and b'rd=emmc0s64' in cmdline, 'Unexpected command line')
    adt_off, adt_size = struct.unpack_from('<2Q', prior, 0x20)
    require(adt_off + adt_size <= len(prior), 'Invalid ADT range')
    root, _ = adt.parse(prior[adt_off:adt_off + adt_size])
    chosen = next(node for node in root.children if node.name == 'chosen')
    mmap = next(node for node in chosen.children if node.name == 'memory-map')
    vbase, = struct.unpack_from('<Q', prior, 0x10)
    phys, = struct.unpack_from('<Q', prior, 0x38)
    blobs = {}
    for name, flags, value in mmap.props:
        if name not in NAMES:
            continue
        require(name not in blobs and len(value) == 16, 'Duplicate/invalid blob mapping')
        addr, size = struct.unpack('<2Q', value)
        va = addr - phys + vbase
        for sva, vs, off, n in segments:
            if sva <= va and va + size <= sva + n:
                blobs[name] = prior[off + va - sva:off + va - sva + size]
                break
        else:
            raise ValueError('Unmapped blob ' + name)
    require(set(blobs) == set(NAMES), 'Missing required firmware/files')
    replacement = args.clock_helper.read_bytes()
    require(replacement[:4] == b'\xcf\xfa\xed\xfe', 'Expected Mach-O clock helper')
    blobs['RiverFiles'] = replace_helper(blobs['RiverFiles'], replacement)
    mmap.props = [prop for prop in mmap.props if prop[0] not in NAMES]
    with tempfile.TemporaryDirectory(prefix='river-repack-') as tmp:
        folder = Path(tmp)
        adt_path = folder / 'river.adt'
        adt_path.write_bytes(adt.serialize(root))
        specs = []
        for name in NAMES:
            path = folder / name
            path.write_bytes(blobs[name])
            specs.append((name, str(path)))
        payload, _ = mkpayload.build(str(args.kernel), str(adt_path), cmdline.decode(), blobs=specs)
    # Independently compare every rebased kernel segment with its packed data.
    kernel = bytearray(args.kernel.read_bytes())
    kernel_hash = digest(kernel)
    macho = mkpayload.parse_macho(kernel)
    mkpayload.apply_chained_fixups(kernel, macho)
    nsegs, = struct.unpack_from('<I', payload, 12)
    packed = {struct.unpack_from('<4Q', payload, 0x2a8 + i * 32)[0]:
              struct.unpack_from('<4Q', payload, 0x2a8 + i * 32) for i in range(nsegs)}
    for seg in macho['segs']:
        if not seg['vmsize']:
            continue
        va, vs, off, n = packed[seg['vmaddr']]
        require(n == seg['filesize'] and payload[off:off + n] ==
                kernel[seg['fileoff']:seg['fileoff'] + n], 'Kernel segment mismatch')
    shim = base[page:page + ks]
    header = bytearray(base[:page])
    struct.pack_into('<I', header, 16, len(payload))
    sha1 = hashlib.sha1()
    for data in (shim, payload, b'', b''):
        sha1.update(data)
        sha1.update(struct.pack('<I', len(data)))
    header[576:608] = sha1.digest().ljust(32, b'\0')
    output = (header + shim.ljust((ks + page - 1) // page * page, b'\0') +
              payload.ljust((len(payload) + page - 1) // page * page, b'\0'))
    require(len(output) < 38000000, 'Boot image exceeds 38 MB')
    fd = os.open(args.output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, 'wb') as stream:
        stream.write(output)
    manifest = dict(kernel_sha256=kernel_hash, base_sha256=digest(base),
                    image_sha256=digest(output), image_bytes=len(output),
                    cmdline_bytes=len(cmdline), kernel_payload_segments_verified=True,
                    clock_helper_sha256=digest(replacement), temporary_boot_only=True)
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
