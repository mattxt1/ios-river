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
    patch = (root / "xnu/patches/0274-river-codec-identity-probe.patch").read_text()
    section = patch.split("diff --git a/bsd/dev/river_codec_probe.h", 1)[1]
    added = "\n".join(line[1:] for line in section.splitlines()
                       if line.startswith("+") and not line.startswith("+++")) + "\n"
    dev = Path(tmp) / "bsd/dev"
    dev.mkdir(parents=True)
    (dev / "river_codec_probe.h").write_text(added)
    subprocess.run(["git", "apply", "--include=bsd/dev/river_codec_probe.h",
                    str(root / "xnu/patches/0275-river-codec-clock-test.patch")], cwd=tmp, check=True)
    added = (dev / "river_codec_probe.h").read_text()
    # Exercise the probe itself; the sysctl wrappers use XNU-only types.
    added = added.split("static int\nriver_codec_probe_sysctl", 1)[0]
    (Path(tmp) / "codec-probe-added.h").write_text(added)
    binary = Path(tmp) / "codec-probe"
    subprocess.run(["cc", "-O2", "-Wall", "-Wextra", "-Werror", "-I", tmp,
                    str(root / "tools/tests/river-codec-probe-host.c"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
    patch = (root / "xnu/patches/0275-river-codec-clock-test.patch").read_text()
    section = patch.split("diff --git a/bsd/dev/river_codec_clock.h", 1)[1]
    added = "\n".join(line[1:] for line in section.splitlines()
                       if line.startswith("+") and not line.startswith("+++")) + "\n"
    (dev / "river_codec_clock.h").write_text(added)
    subprocess.run(["git", "apply", "--include=bsd/dev/river_codec_clock.h",
                    str(root / "xnu/patches/0276-river-codec-clock-restore.patch")], cwd=tmp, check=True)
    subprocess.run(["git", "apply", "--include=bsd/dev/river_codec_clock.h",
                    str(root / "xnu/patches/0277-river-mclk-route-test.patch")], cwd=tmp, check=True)
    patch = (root / "xnu/patches/0278-river-muted-i2s-clock-test.patch").read_text()
    section = patch.split("diff --git a/bsd/dev/river_codec_i2s.h", 1)[1]
    i2s = "\n".join(line[1:] for line in section.splitlines()
                      if line.startswith("+") and not line.startswith("+++")) + "\n"
    (Path(tmp) / "river_codec_i2s.h").write_text(i2s)
    subprocess.run(["git", "apply", "--include=bsd/dev/river_codec_clock.h",
                    str(root / "xnu/patches/0278-river-muted-i2s-clock-test.patch")], cwd=tmp, check=True)
    (dev / "river_codec_i2s.h").write_text(i2s)
    subprocess.run(["git", "apply", str(root / "xnu/patches/0279-river-i2s-snapshot-diagnostics.patch")], cwd=tmp, check=True)
    (Path(tmp) / "river_codec_i2s.h").write_text((dev / "river_codec_i2s.h").read_text())
    added = (dev / "river_codec_clock.h").read_text()
    added = added.split("static int\nriver_codec_clock_sysctl", 1)[0]
    (Path(tmp) / "codec-clock-added.h").write_text(added)
    binary = Path(tmp) / "codec-clock"
    subprocess.run(["cc", "-O2", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function", "-I", tmp,
                    str(root / "tools/tests/river-codec-clock-host.c"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
    section = patch.split("diff --git a/bsd/dev/river_codec_clock.h", 1)[0]
    added = "\n".join(line[1:] for line in section.splitlines()
                       if line.startswith(("+", " ")) and not line.startswith("+++"))
    # Include unchanged context to retain the public wrapper signature.
    # The complete added volume implementation and its locking wrapper.
    volume = added.split("static void\nriver_speaker_set_volume_locked", 1)[1]
    volume = "static void\nriver_speaker_set_volume_locked" + volume.split("static int\nriver_speaker_tone_locked", 1)[0]
    (Path(tmp) / "speaker-volume-added.h").write_text(volume)
    binary = Path(tmp) / "speaker-volume"
    subprocess.run(["cc", "-O2", "-Wall", "-Wextra", "-Werror", "-I", tmp,
                    str(root / "tools/tests/river-speaker-volume-host.c"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
