#!/usr/bin/env python3
import copy
import plistlib
from river_camera_service import JOB, SERVICE, prepare_capture_removal, validate_capture_removal


def raw(data):
    return plistlib.dumps(data, fmt=plistlib.FMT_BINARY)


before = {'LaunchDaemons': {JOB: {'Label': 'com.apple.mediaserverd',
          'ProgramArguments': ['/usr/sbin/mediaserverd'],
          'MachServices': {SERVICE: True, 'com.apple.audio.AudioSession': True,
                           'com.apple.coremedia.capturesession': True}},
          '/other': {'Label': 'other', 'KeepAlive': True}}, 'VersionNumber': 1}
configs = {name: raw({'StartRecorderServer': False, 'StartWirelessDisplayServer': True})
           for name in ('D22', 'D221', 'Default')}
after = prepare_capture_removal(raw(before), configs)
actual = plistlib.loads(after)
assert SERVICE not in actual['LaunchDaemons'][JOB]['MachServices']
restored = copy.deepcopy(actual)
restored['LaunchDaemons'][JOB]['MachServices'][SERVICE] = True
assert restored == before


def refused(call):
    try:
        call()
    except (ValueError, KeyError):
        return
    raise AssertionError('Unexpected configuration accepted')


for name in configs:
    changed = dict(configs)
    changed[name] = raw({'StartRecorderServer': True})
    refused(lambda: prepare_capture_removal(raw(before), changed))
refused(lambda: prepare_capture_removal(raw(before), {}))
refused(lambda: prepare_capture_removal(after, configs))
for mutate in (
        lambda d: d['LaunchDaemons'][JOB]['MachServices'].pop('com.apple.audio.AudioSession'),
        lambda d: d['LaunchDaemons'][JOB]['MachServices'].pop('com.apple.coremedia.capturesession'),
        lambda d: d['LaunchDaemons']['/other'].update(KeepAlive=False),
        lambda d: d.update(VersionNumber=2)):
    altered = copy.deepcopy(actual)
    mutate(altered)
    refused(lambda: validate_capture_removal(raw(before), raw(altered)))
print('PASS capture-only launchd edit; all audio/other services preserved; enabled/missing recorder configuration and unrelated edits refused')
