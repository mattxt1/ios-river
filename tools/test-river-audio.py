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
    patch = (root / "xnu/patches/0273-river-audio-prerequisites.patch").read_text()
    section = patch.split("diff --git a/bsd/sys/hakios.audio.h", 1)[0]
    added = "\n".join(line[1:] for line in section.splitlines()
                       if line.startswith("+") and not line.startswith("+++")) + "\n"
    (Path(tmp) / "audio-survey-added.h").write_text(added)
    binary = Path(tmp) / "audio-survey"
    subprocess.run(["cc", "-O2", "-Wall", "-Wextra", "-Werror", "-I", tmp,
                    str(root / "tools/tests/river-audio-survey-host.c"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
