#!/usr/bin/env python3
"""Run bounded UI replays on River over SSH. Stop and preserve evidence on failure."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from contextlib import contextmanager
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import time
import zlib

UI_SHA = "51c940fd789134e543fa6e382ce05721bcb53c3f3e51cb15509ebbb450146012"
BIN = "/usr/local/bin/"
TRACE = BIN + "river-fb-trace344"
CANVAS = (0, 0, 1125, 2436)
PANEL = (1080, 2270)
PROCESSES = ("backboardd", "SpringBoard", "Spotlight", "searchd")


def utc():
    return datetime.now(timezone.utc).isoformat()


def panel_point(x, y, viewport):
    if not (0 <= x < PANEL[0] and 0 <= y < PANEL[1]):
        raise ValueError("gesture coordinate is outside the panel")
    if viewport == "crop":
        return (x + 20) / 1125, (y + 83) / 2436
    logical_x = max(0, min(1124, (x - 16) * 1125 // 1048))
    return logical_x / 1125, (y * 2436 // 2270) / 2436


def gesture_args(values, viewport):
    x, y, x2, y2, duration = values
    if not 32 <= duration <= 1200:
        raise ValueError("gesture duration must be 32–1200 ms")
    points = panel_point(x, y, viewport) + panel_point(x2, y2, viewport)
    args = ["gesture", *(f"{point:.7f}" for point in points), str(duration)]
    if min(x, y, PANEL[0] - 1 - x, PANEL[1] - 1 - y) < 48:
        args.append("edge")
    return args


FRAME_FORMATS = ((b"P5\n225 487\n255\n", 225, 487, 1),
                 (b"P6\n375 812\n255\n", 375, 812, 3))


def frame_signature(frame):
    for header, width, height, channels in FRAME_FORMATS:
        if frame.startswith(header) and len(frame) == len(header) + width * height * channels:
            break
    else:
        raise ValueError("invalid 0349 screen frame")
    pixels = memoryview(frame)[len(header):]
    # Ignore the status clock and dock. Match the icon and search-field region.
    signature = []
    for y in range(56, 520, 5):
        row = (y * height // 812) * width
        for x in range(12, 363, 5):
            i = channels * (row + x * width // 375)
            brightness = pixels[i] if channels == 1 else (pixels[i] + 2 * pixels[i + 1] + pixels[i + 2]) / 4
            signature.append(brightness / 255)
    return signature


def reference_state(frame, refs):
    signature = frame_signature(frame)
    distances = sorted((sum(abs(a - b) for a, b in zip(signature, ref)) / len(signature), name)
                       for name, ref in refs.items())
    best, name = distances[0]
    return name if best < .14 and distances[1][0] - best > .035 else "unknown"


class NoPresentation(RuntimeError):
    pass


class River:
    def __init__(self, args, directory):
        self.directory = directory
        self.refs = {name: frame_signature(path.read_bytes()) for name, path in
                     (("home", args.home_reference), ("spotlight", args.spotlight_reference)) if path}
        self.ssh = ["ssh", "-F", "/dev/null", "-o", "Compression=yes", "-o", "BatchMode=yes", "-o", "IdentitiesOnly=yes",
                    "-o", "IdentityAgent=none", "-o", "StrictHostKeyChecking=yes",
                    "-o", "HostKeyAlias=192.168.7.2", "-o", "ConnectTimeout=5",
                    "-o", "ServerAliveInterval=5", "-o", "ServerAliveCountMax=4",
                    "-i", str(args.identity), f"root@{args.host}"]
        data = args.helper.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        self.helper = f"/private/var/tmp/river-ui-replay-{digest[:12]}"
        sha = self.read("installed-ui-sha", [BIN + "river-file-sha339", "/usr/local/lib/libcgprobe.dylib"])
        if sha.split()[:1] != [args.ui_sha.encode()]:
            raise RuntimeError("installed SpringBoard library does not match the selected candidate")
        self.read("helper-stage", [BIN + "rput", self.helper, "0755"], data=data)
        installed = self.read("helper-sha", [BIN + "river-file-sha339", self.helper])
        if installed.split() != [digest.encode(), str(len(data)).encode()]:
            raise RuntimeError("deployed helper does not match the local binary")

    def read(self, name, argv, timeout=18, data=None):
        command = argv if isinstance(argv, str) else shlex.join(argv)
        with (self.directory / name).open("xb") as out, (self.directory / f"{name}.stderr").open("xb") as err:
            r = subprocess.run(self.ssh + [command], input=data, stdin=subprocess.DEVNULL if data is None else None,
                               stdout=out, stderr=err, timeout=timeout)
        if r.returncode:
            raise RuntimeError(f"{name} exited {r.returncode}; see its stderr")
        return (self.directory / name).read_bytes()

    def mark(self, name, **fields):
        with (self.directory / "events.jsonl").open("a") as out:
            out.write(json.dumps({"utc": utc(), "event": name, **fields}) + "\n")

    def snapshot(self, name, gesture=None, observation_seconds=1.25):
        self.read(f"{name}-trace-start", [TRACE, "--start"], timeout=10)
        try:
            if gesture:
                self.mark("gesture-start", step=name, gesture=gesture)
                self.read(f"{name}-gesture", [self.helper, *gesture], timeout=8)
                self.mark("gesture-return", step=name)
            time.sleep(observation_seconds)
        finally:
            self.read(f"{name}-trace-stop", [TRACE, "--stop"], timeout=10)
        trace = json.loads(self.read(f"{name}-trace.json", [TRACE, "--read"], timeout=12))
        records = trace["records"]
        if not records:
            raise NoPresentation(f"{name}: no new presentation attempt")
        latest = records[-1]
        layer = latest["layers"][0]
        if (latest["mask"] != 1 or tuple(layer["source"]) != CANVAS or
                tuple(layer["destination"]) != CANVAS or layer["surface"] <= 0):
            raise RuntimeError(f"{name}: last attempt is not a full single-layer canvas")
        packed = self.read(f"{name}.pgm.zlib", [self.helper, "screen", str(layer["surface"])], timeout=32)
        decoder = zlib.decompressobj()
        frame = decoder.decompress(packed, 1_000_000)
        if not decoder.eof or decoder.unused_data:
            raise RuntimeError(f"{name}: incomplete or invalid frame transfer")
        state = reference_state(frame, self.refs) if self.refs else "unclassified"
        (self.directory / f"{name}.pgm").write_bytes(frame)
        self.mark("observed", step=name, state=state, surface=layer["surface"], swap=latest["swap"],
                  presents=len(records), device_time_ns=latest["time_ns"])
        print(f"{name}: {state} (surface {layer['surface']}, {len(records)} present attempts)", flush=True)
        return state

    def diagnose(self):
        self.mark("diagnostics-start")

        def capture(name, command, timeout):
            try:
                return self.read(name, command, timeout=timeout)
            except (OSError, RuntimeError, subprocess.TimeoutExpired) as e:
                print(f"{name}: {e}", flush=True)
                return b""

        with ThreadPoolExecutor(max_workers=2) as pool:
            kernel = pool.submit(capture, "failure-kernel.txt", [BIN + "river-kmsg"], 30)
            rows = capture("failure-processes.txt", [BIN + "psx"], 12)
            pending = {}
            for row in rows.decode(errors="replace").splitlines():
                fields = row.split()
                if len(fields) != 5 or not fields[0].isdecimal() or fields[-1] not in PROCESSES:
                    continue
                pid, name = fields[0], fields[-1]
                for kind, helper in (("kcdata", [self.helper, "stackshot"]), ("txt", [BIN + "pstack"])):
                    pending[name, kind] = pool.submit(capture, f"failure-{name}-{pid}.{kind}",
                                                       [*helper, pid], 16)
            results = {key: bool(job.result()) for key, job in pending.items()}
            kernel_ok = bool(kernel.result())
        ok = kernel_ok and all(results.get((name, kind), False)
                               for name in PROCESSES[:2] for kind in ("kcdata", "txt"))
        self.mark("diagnostics-end", kernel=kernel_ok, files_with_data=sum(results.values()),
                  attempted=len(results), core_capture_complete=ok)
        return ok



@contextmanager
def panel_video(river, camera, fps):
    if camera is None:
        yield
        return
    # The selected Mac camera must physically face the panel. No audio input.
    with (river.directory / "panel-video.stderr").open("xb") as log:
        command = ["ffmpeg", "-hide_banner", "-loglevel", "warning", "-f", "avfoundation",
                   "-framerate", str(fps), "-i", f"{camera}:none", "-an", "-c:v", "libx264", "-preset", "ultrafast", "-crf", "20",
                   "-pix_fmt", "yuv420p", "-movflags", "+faststart", str(river.directory / "panel.mp4")]
        child = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=log)
        river.mark("camera-start", source=camera, fps=fps)
        try:
            time.sleep(1)
            if child.poll() is not None:
                raise RuntimeError("camera could not start; see panel-video.stderr")
            yield
            if child.poll() is not None:
                raise RuntimeError("camera stopped during replay; see panel-video.stderr")
        finally:
            try:
                if child.poll() is None:
                    child.communicate(b"q\n", timeout=8)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait()
            river.mark("camera-end", exit=child.returncode)


def replay(args, river):
    try:
        state = river.snapshot("baseline")
    except NoPresentation:
        if not args.confirmed_home or args.mode not in ("spotlight", "gesture"):
            raise
        state = "home"
        river.mark("baseline-confirmed-home", reason="no new attempt on idle screen")
    if args.mode == "observe":
        return
    home = gesture_args((540, 990, 540, 990, 50), args.viewport)
    if state == "spotlight" and args.mode == "spotlight" and args.start_spotlight:
        state = river.snapshot("initial-return-home", home)
    if state != "home":
        raise RuntimeError("active replay requires a verified Home screen")
    if args.mode == "gesture":
        if river.snapshot("gesture", gesture_args(args.points, args.viewport)) != args.expect:
            raise RuntimeError(f"gesture did not reach {args.expect}")
        return
    for attempt in range(1, args.runs + 1):
        down = gesture_args((540, 635, 540, 1450, 320), args.viewport)
        if river.snapshot(f"{attempt:02}-open-spotlight", down) != "spotlight":
            raise RuntimeError("Spotlight did not reach the Search field and keyboard")
        if river.snapshot(f"{attempt:02}-return-home", home) != "home":
            raise RuntimeError("Spotlight did not return to Home")
    print(f"Completed {args.runs} Spotlight open/close cycles.", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("spotlight", "observe", "capture", "gesture"))
    parser.add_argument("--helper", type=Path, required=True)
    parser.add_argument("--home-reference", type=Path)
    parser.add_argument("--spotlight-reference", type=Path)
    parser.add_argument("--confirmed-home", action="store_true", help="Use a just-confirmed physical Home if idle produces no new frame")
    parser.add_argument("--start-spotlight", action="store_true", help="Dismiss a verified Spotlight baseline before replay")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--identity", type=Path, required=True)
    parser.add_argument("--host", choices=("192.168.1.232", "192.168.7.2"), default="192.168.1.232")
    parser.add_argument("--ui-sha", default=UI_SHA)
    parser.add_argument("--runs", type=int, default=2, choices=range(1, 11))
    parser.add_argument("--points", nargs=5, type=int, metavar=("X", "Y", "X2", "Y2", "MS"))
    parser.add_argument("--expect", choices=("home", "spotlight"))
    parser.add_argument("--viewport", choices=("crop", "fit"), default="crop")
    parser.add_argument("--camera", type=int, help="AVFoundation video input index for an external panel recording")
    parser.add_argument("--camera-fps", type=int, choices=(24, 25, 30, 50, 60), default=30)
    args = parser.parse_args()
    if args.camera is not None and (args.camera < 0 or args.mode == "capture"):
        parser.error("--camera must be a nonnegative video input index on a replay or observe run")
    if bool(args.home_reference) != bool(args.spotlight_reference):
        parser.error("supply both reference frames together")
    if args.mode in ("spotlight", "gesture") and not args.home_reference:
        parser.error("screen checks require both reference frames")
    if args.mode == "gesture" and (not args.points or not args.expect):
        parser.error("gesture requires --points and --expect")
    if args.points:
        try:
            gesture_args(args.points, args.viewport)
        except ValueError as e:
            parser.error(str(e))
    os.umask(0o077)
    directory = args.output.resolve()
    directory.mkdir(parents=True, exist_ok=False)
    print(f"Evidence: {directory}", flush=True)
    river = River(args, directory)
    if args.mode == "capture":
        if not river.diagnose():
            raise SystemExit("diagnostics incomplete; check evidence")
        return
    try:
        with panel_video(river, args.camera, args.camera_fps):
            replay(args, river)
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as e:
        river.mark("failure", error=str(e))
        print(f"Replay stopped: {e}", flush=True)
        river.diagnose()
        raise SystemExit(1) from e


if __name__ == "__main__":
    main()
