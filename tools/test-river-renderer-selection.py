#!/usr/bin/env python3
"""Exercise the actual repacker selection without private image inputs."""
import ast
from pathlib import Path
import re

source = Path(__file__).with_name('river-repack-audio-diagnostics.py')
tree = ast.parse(source.read_text())
nodes = [n for n in tree.body if isinstance(n, ast.FunctionDef)
         and n.name in ('require', 'select_renderer')]
ns = {'re': re}
exec(compile(ast.Module(body=nodes, type_ignores=[]), str(source), 'exec'), ns)
select = ns['select_renderer']
token = b'riverenv=backboardd:DYLD_INSERT_LIBRARIES=/usr/local/lib/sweetgl.dylib'
base = b'rd=emmc0s64 ' + token + b' rootrw=1 msgbuf=1048576'
result = select(base, 'sweetgl-p0339.dylib')
assert result.replace(b'sweetgl-p0339.dylib', b'sweetgl.dylib') == base
for cmd, name in [(base, '../sweetgl.dylib'), (base, 'sweetgl.dylib'),
                  (base, 'sweetgl-p0339.dylib extra=1'),
                  (base + b' ' + token, 'sweetgl-p0339.dylib'),
                  (base + b' riverenv=x:DYLD_INSERT_LIBRARIES=x', 'sweetgl-p0339.dylib'),
                  (b'rd=emmc0s64', 'sweetgl-p0339.dylib'),
                  (base + b' ' * 608, 'sweetgl-p0339.dylib')]:
    try:
        select(cmd, name)
    except ValueError:
        pass
    else:
        raise AssertionError((cmd, name))
print('PASS renderer selection preserves unrelated boot arguments and rejects ambiguity/overflow')
