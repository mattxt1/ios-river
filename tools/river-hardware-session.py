#!/usr/bin/env python3
"""Verified 0349 temporary boot and opt-in read-only device captures.

No flashing, audio setup, brightness writes, haptic requests, daemon restarts
or charger writes. 'collect' stages one hash-verified read-only helper.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import struct
import subprocess
import time

IMAGE_SHA = '9e1e80630e7eb85424f34fd08fe4beca5659137dc9c094c02361b1fc3bddf4b8'
ROLLBACK_SHA = '5abd9cfbfe7aef83b9ea68dbd14e7a4eebc2b8b09f1caa66e62a42532f5e9415'
HELPER_SHA = 'c987cc190c9a8a5bb120fe4b483f0f23bb81ce1d1971b17e14217c6e748d145f'
UI_SHA = '51c940fd789134e543fa6e382ce05721bcb53c3f3e51cb15509ebbb450146012'
SERIAL = 'ZY226HM8QB'


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def verified_bytes(path, expected):
    data = path.read_bytes()
    require(hashlib.sha256(data).hexdigest() == expected, 'Hash mismatch: ' + str(path))
    return data


def preflight(manifest):
    m = json.loads(manifest.read_text())
    require(m['image_revision'] == 349 and m['temporary_boot_only'], 'Wrong candidate')
    require(m['image_sha256'] == IMAGE_SHA and m['rollback_sha256'] == ROLLBACK_SHA,
            'Manifest does not match the pinned candidate and rollback')
    data = verified_bytes(Path(m['image']), IMAGE_SHA)
    verified_bytes(Path(m['rollback_image']), ROLLBACK_SHA)
    require(len(data) == m['image_bytes'] and len(data) < 38_000_000, 'Image size invalid')
    require(data[:8] == b'ANDROID!', 'Invalid Android boot image')
    kernel_size = struct.unpack_from('<I', data, 8)[0]
    ramdisk_size = struct.unpack_from('<I', data, 16)[0]
    page = struct.unpack_from('<I', data, 36)[0]
    require(page in (2048, 4096, 8192, 16384), 'Unexpected page size')
    offset = page + (kernel_size + page - 1) // page * page
    require(offset + ramdisk_size <= len(data), 'Truncated payload')
    payload = data[offset:offset + ramdisk_size]
    require(payload[:8] == b'SWEETXNU', 'Wrong payload format')
    line = payload[0x48:0x2a8].split(b'\0')[0]
    require(len(line) == m['cmdline_bytes'] and len(line) < 608, 'Invalid XNU command line')
    tokens = line.split()
    require(tokens.count(b'riverfeedback=1') == 1 and b'riverhaptics=0' not in tokens,
            'Native feedback mode not enabled')
    require(b'riverbl=1' in tokens, 'Backlight mode not enabled')
    verified_bytes(manifest.parent / 'river-hardware-check', HELPER_SHA)
    return m, {'image_sha256': IMAGE_SHA, 'rollback_sha256': ROLLBACK_SHA,
               'image_bytes': len(data), 'xnu_cmdline_bytes': len(line),
               'native_feedback_enabled': True, 'haptics_default_enabled': True}


def run_logged(directory, name, argv, timeout=20, data=None):
    started = time.monotonic()
    try:
        r = subprocess.run(argv, input=data, stdin=subprocess.DEVNULL if data is None else None,
                           capture_output=True, timeout=timeout)
    except subprocess.TimeoutExpired as e:
        (directory / (name + '.txt')).write_bytes((e.stdout or b'') + (e.stderr or b''))
        raise RuntimeError(name + ' timed out; no dependent action sent') from e
    (directory / (name + '.txt')).write_bytes(r.stdout + r.stderr)
    (directory / (name + '.json')).write_text(json.dumps({
        'exit_code': r.returncode, 'elapsed_seconds': time.monotonic() - started,
        'utc': datetime.now(timezone.utc).isoformat()}, indent=2) + '\n')
    require(r.returncode == 0, name + ' failed; inspect ' + str(directory / (name + '.txt')))
    return r.stdout


def collect(directory, manifest, host, stacks):
    key = Path('/Users/matttyson/Developer/Codex/.device-ssh/id_ed25519')
    require(key.is_file(), 'Configured device SSH identity is unavailable')
    ssh = ['ssh', '-F', '/dev/null', '-o', 'BatchMode=yes', '-o', 'IdentitiesOnly=yes',
           '-o', 'IdentityAgent=none', '-o', 'StrictHostKeyChecking=yes',
           '-o', 'HostKeyAlias=192.168.7.2', '-o', 'ConnectTimeout=5',
           '-o', 'ServerAliveInterval=5', '-o', 'ServerAliveCountMax=2',
           '-i', str(key), 'root@' + host]

    def capture(name, command, timeout=20):
        return run_logged(directory, name, ssh + [command], timeout)

    # Verify the installed candidate before staging or making capability probes.
    sha = capture('installed-ui-sha', '/usr/local/bin/river-file-sha339 /usr/local/lib/libcgprobe.dylib')
    require(sha.decode().split()[0] == UI_SHA, 'Installed UI is not the 0349 candidate')
    capture('kernel-before', '/usr/local/bin/river-kmsg', 30)
    capture('identity', 'uname -a')
    processes = capture('processes-start', '/usr/local/bin/psx').decode()
    if stacks:
        for row in processes.splitlines():
            fields = row.split()
            if len(fields) == 5 and fields[0].isdigit() and fields[4] in ('SpringBoard', 'backboardd', 'Preferences'):
                capture('stack-' + fields[4], '/usr/local/bin/pstack ' + fields[0], 10)
        capture('kernel-after', '/usr/local/bin/river-kmsg', 30)
        return
    for name, option in [('ios', '--ios-status'), ('queue', '--queue-status'),
                         ('auto', '--auto-status'), ('mic', '--mic-status'),
                         ('mic-live', '--mic-live-status'), ('start', '--start-status')]:
        capture('audio-read-only-' + name, '/usr/local/bin/river-adsp-status ' + option)
    remote = '/usr/local/bin/river-hardware-check-0349-' + HELPER_SHA[:12]
    helper = verified_bytes(manifest.parent / 'river-hardware-check', HELPER_SHA)
    run_logged(directory, 'stage-read-only-helper', ssh + ['/usr/local/bin/rput ' + remote + ' 0755'], data=helper)
    readback = capture('helper-readback', '/usr/local/bin/rget ' + remote + ' 0 ' + str(len(helper)))
    require(readback == helper, 'Helper readback differs; helper will not be executed')
    report = json.loads(capture('hardware-read-only', remote))
    require(report.get('native_mode') == 1 and report.get('action_requested') == 'none',
            'Unexpected native mode/report')
    capture('battery-iops', '/usr/local/bin/battcheck')
    time.sleep(10)
    capture('processes-end', '/usr/local/bin/psx')
    capture('hardware-after', remote)
    capture('kernel-after', '/usr/local/bin/river-kmsg', 30)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('action', choices=('preflight', 'boot', 'collect', 'stacks'))
    default = Path(__file__).resolve().parents[3] / 'outputs/hardware/candidate-manifest.json'
    ap.add_argument('--manifest', type=Path, default=default)
    ap.add_argument('--host', choices=('192.168.7.2', '192.168.1.232'), default='192.168.7.2')
    ap.add_argument('--home-ready', action='store_true', help='Fresh navigation and keyboard confirmed before probes')
    args = ap.parse_args()
    os.umask(0o077)
    manifest = args.manifest.resolve()
    m, summary = preflight(manifest)
    directory = manifest.parent / 'device-0349' / (datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ') + '-' + args.action)
    directory.mkdir(parents=True, mode=0o700)
    (directory / 'preflight.json').write_text(json.dumps(summary, indent=2) + '\n')
    print('Verified candidate, rollback, helper and XNU command line.', flush=True)
    print('Evidence:', directory, flush=True)
    if args.action == 'boot':
        devices = run_logged(directory, 'fastboot-devices', ['fastboot', 'devices', '-l']).decode()
        require(any(row.split()[:2] == [SERIAL, 'fastboot'] for row in devices.splitlines()),
                'Expected River is not accessible to fastboot; nothing booted')
        run_logged(directory, 'product', ['fastboot', '-s', SERIAL, 'getvar', 'product'], 5)
        product = (directory / 'product.txt').read_text()
        require(re.search(r'(?m)^\s*(?:\(bootloader\)\s*)?product:\s*river\s*$', product), 'Wrong product')
        run_logged(directory, 'temporary-boot', ['fastboot', '-s', SERIAL, 'boot', m['image']], 45)
        print('Temporary boot command accepted. Confirm home/navigation/keyboard before collecting.', flush=True)
        print('Next:', shlex.join(['python3', str(Path(__file__).resolve()), 'collect', '--home-ready', '--manifest', str(manifest)]))
    elif args.action in ('collect', 'stacks'):
        require(args.home_ready, 'Confirm fresh home/navigation/keyboard, then use --home-ready')
        collect(directory, manifest, args.host, args.action == 'stacks')
        print('Read-only capture completed; tactile/visual behavior still needs user confirmation.')


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, OSError, ValueError, KeyError, IndexError) as error:
        raise SystemExit(str(error))
