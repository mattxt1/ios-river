#!/usr/bin/env python3
"""Exercise actual media ioctl code for native DHCP and 32/64-bit list callers."""
from pathlib import Path
import os,subprocess,tempfile
repo=Path(__file__).resolve().parent.parent
patch=(repo/'xnu/patches/0322-river-wifi-valid-media-link-state.patch').read_text().split('diff --git a/bsd/dev/river_wifi_media.h',1)[1]
source='\n'.join(s[1:] for s in patch.splitlines() if s.startswith('+') and not s.startswith('+++'))+'\n'
with tempfile.TemporaryDirectory(prefix='river-media-') as d:
 root=Path(d);(root/'actual.h').write_text(source)
 subprocess.run([os.environ.get('CC','cc'),'-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(root),str(repo/'tools/tests/river-wifi-media-host.c'),'-o',str(root/'test')],check=True)
 subprocess.run([str(root/'test')],check=True)
