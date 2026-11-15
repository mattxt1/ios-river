#!/usr/bin/env python3
"""Check bounded readiness/republication after DHCP without networking."""
from pathlib import Path
import subprocess,tempfile,os
repo=Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix='river-net-settle-') as d:
 binary=Path(d)/'test'
 subprocess.run([os.environ.get('CC','cc'),'-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(repo/'phone-tools'),str(repo/'tools/tests/river-net-settle-host.c'),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
