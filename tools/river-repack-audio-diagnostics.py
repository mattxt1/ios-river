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
import re
from pathlib import Path
import struct
import tempfile

import adt
import mkpayload
from river_camera_service import validate_capture_removal

NAMES = ('RiverGPUFW', 'RiverWCNSSMeta', 'RiverWCNSSImage', 'RiverWCNSSNV', 'RiverFiles')


def select_native_feedback(cmdline):
    tokens = cmdline.split(b' ')
    require(not any(t.startswith(b'riverfeedback=') for t in tokens), 'Native feedback mode already specified')
    result = cmdline + b' riverfeedback=1'
    require(len(result) < 608, 'Native feedback command line exceeds fixed field')
    return result


def select_renderer(cmdline, name):
    """Select an independently staged candidate without changing the fallback."""
    require(re.fullmatch(r'sweetgl-p[0-9]{4}\.dylib', name) is not None,
            'Expected a numbered candidate renderer basename')
    old = b'riverenv=backboardd:DYLD_INSERT_LIBRARIES=/usr/local/lib/sweetgl.dylib'
    tokens = cmdline.split(b' ')
    require(tokens.count(old) == 1, 'Expected exactly one original renderer selection')
    require(sum(b'DYLD_INSERT_LIBRARIES=' in t for t in tokens) == 1,
            'Ambiguous renderer environment')
    new = old.rsplit(b'/', 1)[0] + b'/' + name.encode('ascii')
    result = cmdline.replace(old, new, 1)
    require(len(result) < 608, 'Renderer command line exceeds fixed field')
    return result


def digest(data):
    return hashlib.sha256(data).hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def replace_helper(blob, replacement, adsp=None, wifi_monitor=None, net=None, network_state=None, ipconfig=None, wifi_check=None, launchd=None, ui_library=None):
    require(blob[:8] == b'RIVFILES', 'Invalid RiverFiles magic')
    count, = struct.unpack_from('<I', blob, 8)
    require(count <= 64 and 12 + count * 128 <= len(blob), 'Invalid file table')
    entries = []
    found = 0
    adsp_found = 0
    wifi_found = 0
    net_found = 0
    network_state_found = 0
    ipconfig_found = 0
    wifi_check_found = 0
    launchd_found = 0
    ui_found = 0
    for i in range(count):
        name, mode, off, size, reserved = struct.unpack_from('<112sIIII', blob, 12 + 128 * i)
        require(off + size <= len(blob), 'File outside container')
        data = blob[off:off + size]
        if name.split(b'\0')[0] == b'/usr/local/lib/libcgprobe.dylib':
            ui_found += 1
            if ui_library is not None:
                data = ui_library
        if name.split(b'\0')[0] == b'/System/Library/xpc/launchd.plist':
            launchd_found += 1
            if launchd is not None:
                validate_capture_removal(data, launchd)
                data = launchd
        if name.split(b'\0')[0] == b'/usr/local/bin/river-codec-clock':
            found += 1
            data = replacement
        if name.split(b'\0')[0] == b'/usr/local/bin/river-adsp-status':
            adsp_found += 1
            if adsp is not None:
                data = adsp
        if name.split(b'\0')[0] == b'/usr/local/bin/river-wifi-monitor-state':
            wifi_found += 1
            if wifi_monitor is not None:
                data = wifi_monitor
        if name.split(b'\0')[0] == b'/usr/local/bin/river-net':
            net_found += 1
            if net is not None:
                data = net
        if name.split(b'\0')[0] == b'/usr/local/bin/river-network-config-state':
            network_state_found += 1
            if network_state is not None:
                data = network_state
        if name.split(b'\0')[0] == b'/usr/local/bin/river-ipconfig-state':
            ipconfig_found += 1
            if ipconfig is not None:
                data = ipconfig
        if name.split(b'\0')[0] == b'/usr/local/bin/river-wifi-check':
            wifi_check_found += 1
            if wifi_check is not None:
                data = wifi_check
        entries.append((name, mode, data, reserved))
    require(found == 1, 'Expected exactly one existing clock helper')
    require(ui_found <= 1, 'Duplicate UI library')
    if ui_library is not None and not ui_found:
        entries.append((b'/usr/local/lib/libcgprobe.dylib', 0o755, ui_library, 0))
    if launchd is not None:
        require(launchd_found == 1, 'Expected exactly one existing launchd configuration')
    require(adsp_found <= 1, 'Duplicate ADSP helper')
    if adsp is not None and not adsp_found:
        entries.append((b'/usr/local/bin/river-adsp-status', 0o755, adsp, 0))
    require(wifi_found <= 1, 'Duplicate Wi-Fi monitor helper')
    if wifi_monitor is not None and not wifi_found:
        entries.append((b'/usr/local/bin/river-wifi-monitor-state', 0o755, wifi_monitor, 0))
    require(net_found <= 1, 'Duplicate Wi-Fi daemon')
    if net is not None:
        require(net_found == 1, 'Expected existing Wi-Fi daemon')
    require(network_state_found <= 1, 'Duplicate network-state reader')
    if network_state is not None and not network_state_found:
        entries.append((b'/usr/local/bin/river-network-config-state', 0o755, network_state, 0))
    require(ipconfig_found <= 1, 'Duplicate IPConfiguration reader')
    if ipconfig is not None and not ipconfig_found:
        entries.append((b'/usr/local/bin/river-ipconfig-state', 0o755, ipconfig, 0))
    require(wifi_check_found <= 1, 'Duplicate connectivity probe')
    if wifi_check is not None and not wifi_check_found:
        entries.append((b'/usr/local/bin/river-wifi-check', 0o755, wifi_check, 0))
    count = len(entries)
    require(count <= 64, 'Too many RiverFiles entries')
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
    ap.add_argument('--renderer-library-name', help='Already staged sweetgl-pNNNN.dylib; preserves the original fallback library')
    ap.add_argument('--camera-launchd-plist', type=Path,
                    help='Optional launchd configuration changing only the disabled capture-source advertisement')
    ap.add_argument('--ui-library', type=Path, help='Optional signed arm64 UI library; installs at its existing SpringBoard load path')
    ap.add_argument('--ui-library-sha256', help='Required exact fingerprint when carrying a UI library')
    ap.add_argument('--rgb-display-fit', action='store_true',
                    help='Select the RGB scanout fit mode of a fit-capable River kernel')
    ap.add_argument('--native-feedback', action='store_true', help='Opt in to the combined native hardware bridge (requires a bridge-bearing UI library)')
    ap.add_argument('--adsp-helper', type=Path)
    ap.add_argument('--wifi-check-helper', type=Path, help='Optional bounded connectivity probe')
    ap.add_argument('--ipconfig-helper', type=Path, help='Optional read-only native DHCP/media reader')
    ap.add_argument('--network-state-helper', type=Path, help='Optional read-only kernel/SC state reader')
    ap.add_argument('--net-helper', type=Path, help='Optional existing Wi-Fi daemon with publication observations')
    ap.add_argument('--wifi-monitor-helper', type=Path, help='Optional bounded read-only Wi-Fi monitor diagnostics')
    ap.add_argument('--msgbuf-bytes', type=int, choices=(131072, 262144, 1048576),
                    help='Optional bounded kernel log buffer for temporary diagnostics')
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
    if args.native_feedback:
        require(args.ui_library is not None, 'Native feedback requires the combined UI/bridge library')
        cmdline = select_native_feedback(cmdline)
    if args.rgb_display_fit:
        tokens = cmdline.split(b' ')
        require([token for token in tokens if token.startswith(b'riverblit=')] == [b'riverblit=1'],
                'Expected one standard River GPU present argument')
        cmdline = b' '.join(b'riverblit=2' if token == b'riverblit=1' else token for token in tokens)
    if args.renderer_library_name is not None:
        cmdline = select_renderer(cmdline, args.renderer_library_name)
    if args.msgbuf_bytes is not None:
        matches = list(re.finditer(rb'(?<!\S)msgbuf=[0-9]+(?=\s|$)', cmdline))
        require(len(matches) <= 1, 'Duplicate msgbuf arguments')
        require(b'msgbuf=' not in cmdline or len(matches) == 1, 'Unsupported msgbuf argument')
        token = ('msgbuf=%d' % args.msgbuf_bytes).encode('ascii')
        if matches:
            match = matches[0]
            cmdline = cmdline[:match.start()] + token + cmdline[match.end():]
        else:
            cmdline += b' ' + token
        require(len(cmdline) < 608, 'Diagnostic command line exceeds fixed field')
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
    adsp_helper = args.adsp_helper.read_bytes() if args.adsp_helper else None
    if adsp_helper is not None:
        require(adsp_helper[:4] == b'\xcf\xfa\xed\xfe', 'Expected Mach-O ADSP helper')
    wifi_helper = args.wifi_monitor_helper.read_bytes() if args.wifi_monitor_helper else None
    if wifi_helper is not None:
        require(wifi_helper[:4] == b'\xcf\xfa\xed\xfe', 'Expected Mach-O Wi-Fi monitor helper')
    net_helper = args.net_helper.read_bytes() if args.net_helper else None
    if net_helper is not None:
        require(net_helper[:8] == bytes.fromhex('cffaedfe0c000001'), 'Expected arm64 Wi-Fi daemon')
    network_state = args.network_state_helper.read_bytes() if args.network_state_helper else None
    if network_state is not None:
        require(network_state[:8] == bytes.fromhex('cffaedfe0c000001'), 'Expected arm64 network-state reader')
    prior_files = blobs['RiverFiles']
    ipconfig = args.ipconfig_helper.read_bytes() if args.ipconfig_helper else None
    if ipconfig is not None:
        require(ipconfig[:8] == bytes.fromhex('cffaedfe0c000001'), 'Expected arm64 native DHCP reader')
    wifi_check = args.wifi_check_helper.read_bytes() if args.wifi_check_helper else None
    if wifi_check is not None:
        require(wifi_check[:8] == bytes.fromhex('cffaedfe0c000001'), 'Expected arm64 connectivity probe')
    launchd = args.camera_launchd_plist.read_bytes() if args.camera_launchd_plist else None
    ui_library = args.ui_library.read_bytes() if args.ui_library else None
    require(bool(args.ui_library) == bool(args.ui_library_sha256), 'UI library and fingerprint must be supplied together')
    if ui_library is not None:
        require(len(ui_library) >= 32 and ui_library[:8] == bytes.fromhex('cffaedfe0c000001') and
                struct.unpack_from('<I', ui_library, 12)[0] == 6, 'Expected arm64 Mach-O UI dylib')
        require(digest(ui_library) == args.ui_library_sha256.lower(), 'UI library fingerprint mismatch')
    blobs['RiverFiles'] = replace_helper(prior_files, replacement, adsp_helper, wifi_helper, net_helper, network_state, ipconfig, wifi_check, launchd, ui_library)
    # Compare the actual containers: every unrelated file and metadata survives.
    def files(blob):
        result = {}
        for i in range(struct.unpack_from('<I', blob, 8)[0]):
            name, mode, off, size, reserved = struct.unpack_from('<112sIIII', blob, 12+i*128)
            name = name.split(b'\0')[0]
            require(name not in result and off+size <= len(blob), 'Invalid/duplicate file')
            result[name] = (mode, reserved, blob[off:off+size])
        return result
    before, after = files(prior_files), files(blobs['RiverFiles'])
    changed = {b'/usr/local/bin/river-codec-clock': replacement}
    if launchd is not None:
        changed[b'/System/Library/xpc/launchd.plist'] = launchd
    if ui_library is not None:
        changed[b'/usr/local/lib/libcgprobe.dylib'] = ui_library
    for name, data in [(b'/usr/local/bin/river-adsp-status', adsp_helper),
                       (b'/usr/local/bin/river-wifi-monitor-state', wifi_helper),
                       (b'/usr/local/bin/river-net', net_helper),
                       (b'/usr/local/bin/river-network-config-state', network_state),
                       (b'/usr/local/bin/river-ipconfig-state', ipconfig),
                       (b'/usr/local/bin/river-wifi-check', wifi_check)]:
        if data is not None:
            changed[name] = data
    require(set(after) == set(before) | set(changed), 'Unexpected file set')
    for name, (mode, reserved, data) in after.items():
        require(data == (changed[name] if name in changed else before[name][2]), 'File data mismatch')
        if name in before:
            require((mode, reserved) == before[name][:2], 'File metadata mismatch')
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
    manifest['unrelated_riverfiles_verified'] = True
    manifest['native_hardware_bridge_opt_in'] = b'riverfeedback=1' in cmdline.split()
    if args.rgb_display_fit:
        manifest['rgb_display_fit'] = True
    if ui_library is not None:
        manifest['ui_library_sha256'] = digest(ui_library)
        manifest['ui_library_path'] = '/usr/local/lib/libcgprobe.dylib'
    if launchd is not None:
        manifest['camera_launchd_sha256'] = digest(launchd)
        manifest['only_capture_source_advertisement_changed'] = True
    if args.renderer_library_name is not None:
        manifest['staged_renderer_path'] = '/usr/local/lib/' + args.renderer_library_name
    if wifi_check is not None:
        manifest['wifi_check_helper_sha256'] = digest(wifi_check)
    if ipconfig is not None:
        manifest['ipconfig_helper_sha256'] = digest(ipconfig)
    if network_state is not None:
        manifest['network_state_helper_sha256'] = digest(network_state)
    if net_helper is not None:
        manifest['net_helper_sha256'] = digest(net_helper)
    if args.msgbuf_bytes is not None:
        manifest['msgbuf_bytes'] = args.msgbuf_bytes
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
