#!/usr/bin/env python3
"""Exercise the actual container editor's UI add/replace/fallback behavior."""
import ast
from pathlib import Path
import struct

path = Path(__file__).with_name('river-repack-audio-diagnostics.py')
tree = ast.parse(path.read_text())
ns = {'struct': struct}
exec(compile(ast.Module(body=[n for n in tree.body if isinstance(n, ast.FunctionDef)
     and n.name in ('require', 'replace_helper', 'select_native_feedback')], type_ignores=[]), str(path), 'exec'), ns)


def pack(entries):
    off = (12 + 128 * len(entries) + 15) & ~15
    table, content = bytearray(), bytearray()
    for name, mode, data, reserved in entries:
        table += struct.pack('<112sIIII', name, mode, off + len(content), len(data), reserved)
        content += data + b'\0' * (-len(data) & 15)
    return (b'RIVFILES' + struct.pack('<I', len(entries)) + table).ljust(off, b'\0') + content


def unpack(blob):
    result = {}
    for i in range(struct.unpack_from('<I', blob, 8)[0]):
        name, mode, off, size, reserved = struct.unpack_from('<112sIIII', blob, 12 + i * 128)
        result[name.split(b'\0')[0]] = (mode, blob[off:off + size], reserved)
    return result


clock = b'/usr/local/bin/river-codec-clock'
ui = b'/usr/local/lib/libcgprobe.dylib'
other = b'/usr/local/bin/unrelated'
base = pack([(clock, 0o755, b'clock', 7), (other, 0o700, b'unchanged', 9)])
edit = ns['replace_helper']
candidate = edit(base, b'clock', ui_library=b'candidate')
assert unpack(candidate) == {**unpack(base), ui: (0o755, b'candidate', 0)}
rollback = edit(candidate, b'clock', ui_library=b'original')
assert unpack(rollback) == {**unpack(base), ui: (0o755, b'original', 0)}
assert unpack(edit(candidate, b'clock')) == unpack(candidate)
duplicate = pack([(clock, 0o755, b'clock', 0), (ui, 0o755, b'a', 0), (ui, 0o755, b'b', 0)])
try:
    edit(duplicate, b'clock', ui_library=b'candidate')
except ValueError:
    pass
else:
    raise AssertionError('Duplicate UI library accepted')
print('PASS actual UI library add/replace/rollback, duplicate rejection and unrelated file/metadata preservation')
select=ns['select_native_feedback']
assert select(b'rd=emmc0s64')==b'rd=emmc0s64 riverfeedback=1'
assert len(select(b'x'*583))==599
for invalid in [b'riverfeedback=0',b'rd=emmc0s64 riverfeedback=1',b'x'*592]:
    try:select(invalid)
    except ValueError:pass
    else:raise AssertionError('Duplicate/overlength feedback command line accepted')
print('PASS native feedback opt-in preserves existing command line and rejects duplicates/608-byte overflow')
