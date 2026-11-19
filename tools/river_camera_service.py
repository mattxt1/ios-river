"""Narrow launchd edit for a River image with its recorder server disabled."""
import copy
import plistlib

JOB = '/System/Library/LaunchDaemons/com.apple.mediaserverd.plist'
SERVICE = 'com.apple.coremedia.capturesource'


def remove_capture_advertisement(data):
    result = copy.deepcopy(data)
    job = result['LaunchDaemons'][JOB]
    if job.get('Label') != 'com.apple.mediaserverd':
        raise ValueError('Unexpected media job')
    services = job['MachServices']
    if services.get(SERVICE) is not True:
        raise ValueError('Expected original capture-source advertisement')
    if not any(name.startswith('com.apple.audio.') for name in services):
        raise ValueError('Missing normal audio services')
    del services[SERVICE]
    return result


def validate_capture_removal(before, after):
    expected = remove_capture_advertisement(plistlib.loads(before))
    if plistlib.loads(after) != expected:
        raise ValueError('Only the capture-source advertisement may change')


def prepare_capture_removal(before, configs):
    if set(configs) != {'D22', 'D221', 'Default'}:
        raise ValueError('All three recorder configurations are required')
    if any(plistlib.loads(raw).get('StartRecorderServer') is not False
           for raw in configs.values()):
        raise ValueError('Recorder server must already be disabled')
    result = plistlib.dumps(remove_capture_advertisement(plistlib.loads(before)),
                             fmt=plistlib.FMT_BINARY, sort_keys=False)
    validate_capture_removal(before, result)
    return result
