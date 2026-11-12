#!/usr/bin/env python3
"""Compile and run the host PCM fault-injection tests (no phone access)."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix="river-audio-tests-") as tmp:
    binary = Path(tmp) / "audio-io"
    subprocess.run(["cc", "-O2", "-Wall", "-Wextra", "-Werror",
                    str(root / "tools/tests/river-audio-io.c"), "-o", str(binary), "-lm"], check=True)
    subprocess.run([str(binary)], check=True)
