# River battery, backlight and native feedback

The November 19 candidate adds patches 0346–0348 and an opt-in SpringBoard
bridge. It has **not been booted or hardware-validated**. Last known phone
image is 0345. No audio initialization, audio enable, playback or capture was
performed in this task. Device SSH was denied by the host session permissions.

| Area | Candidate implementation | Still required |
| --- | --- | --- |
| Battery reporting | QG voltage/current plus bracketed SMB5 path/state, errors, temperature status, enabled modes and enable-command observations. Publishes raw charger fields and separates external power, enabled charge capability, charging and full. Retains last complete sample on any failed read or detected transition. | Correlated cable/load/recovery traces; measured battery temperature and proper learned fuel-gauge SOC. Percentage remains the existing voltage/current estimate. |
| Charging | Reports disabled, paused, temperature-blocked and faulted charging without claiming that USB power means cell charging. Hard temperature/AFP and battery overvoltage block a capable/charging report. | An explicit charger owner with thermal/JEITA/MMI policy, verified limits and physical charge-current/temperature validation. This candidate does **not enable charging** or write charger controls. |
| Backlight | Native brightness setters/notifications feed the existing DSI DCS 0x51 driver. Driver clamps before arithmetic, reconciles sleep/wake under the DSI lock, and records a sent level only after successful DMA. | Control Center/Settings path coverage, sleep/wake races, failure/retry, brightness range and UI responsiveness on the actual phone. There is no automatic periodic DMA retry. |
| Haptics | UIKit selection, impact and success/warning/error notifications map to bounded AW8691 RAM-waveform pulses. Worker mailbox handles gain, stop and deadlines; native mode suppresses unconditional raw-touch/key pulses. | Actual system call-path coverage, user preference/suppression policy, tactile comparison, keyboard/ringer/alerts and app-wide delivery. Full Core Haptics pattern players, curves, synchronized audio and RTP streaming are **not implemented**. |

The actuator is the probed **AW8691**, chip ID 0x91, on QUP2 address **0x5b**;
older README references to AW8697/0x5a were incorrect. QUP2 remains shared
with the speaker amplifier and uses the existing transaction lock. The new
feedback endpoint does not initialize or enable the audio backend.

## Enabling the candidate

Default boot behavior stays unchanged. The combined `libcgprobe.dylib`
contains the existing 0345 UI code and the new bridge at its current
SpringBoard injection path. `riverfeedback=1` enables native bridge mode;
it does not publish `AppleARMBacklight` or turn on its previously problematic
CoreBrightness digital filter path. `riverbl=1` and `riverhaptics=1` must
already be configured for the underlying drivers.

`debug.river_native_feedback` is a read-only mode value.
`debug.river_haptic` reads readiness without touching I2C. A write encodes
`gain << 16 | milliseconds`; gain is 1–0x28, duration is 1–1000 ms, and zero
stops. Reserved/negative/overrange values are rejected. Requests are queued,
not playback acknowledgements. The existing 10 ms input worker owns AW8691
register writes. The single-slot mailbox uses the latest request, rather
than a lossless pattern queue. Native feedback is therefore an approximation
of the existing RAM waveform, not a Taptic Engine waveform renderer.

The endpoint permits bounded mobile-user requests only when native mode is
enabled. `/usr/local/bin/vibrate` retains its prior privileged interface.
Explicit `RIVER_HW_BRIGHTNESS=0` or `RIVER_HW_FEEDBACK=0` disables that part of
the bridge in an injected process; `1` explicitly enables it. Without an
override, both follow the kernel mode. An environment override does not
override the kernel endpoint's readiness/mode checks.

Only SpringBoard receives the combined candidate library. Injecting it there
does not inject third-party applications or intercept every private UIKit,
BackBoardServices or Core Haptics call. The BKS weak interposer and public
UIScreen hooks need iOS16 runtime confirmation. UIKit hooks forward original
methods, check their type encodings, avoid changing inherited parent methods
and deduplicate nested synchronous calls. The `haptic-feedback` preference
in `com.apple.preferences.sounds` is honored when present, but its exact
relationship to all iOS16 Settings and system suppression policies remains
unverified. Do not describe this as full system haptics integration.

## Build and host validation

```sh
sh tools/build-river-hardware.sh /absolute/output --with-ui
python3 tools/test-river-hardware.py /absolute/applied/xnu
python3 tools/test-river-battery.py
python3 tools/test-river-ui-library-repack.py
python3 tools/test-river-search-switcher.py
python3 tools/test-river-folder-fallback.py
```

The new host runner compiles the actual patched observation/polling,
backlight, haptic endpoint/playback and Objective-C hook functions with
ASan/UBSan, replacing hardware/IOKit boundaries. It checks nine individual
read failures, mapping contention/failure, cable/state transition rejection,
VI sentinels, all eight charger states, hard/soft thermal limits, errors,
command/mode/suspend/DC handling, first-sample absence, stale retention and
recovery. Backlight checks cover stale queued requests after sleep/wake,
duplicate suppression, DMA failure followed by a retry, lock order and
overflow/range bounds. Feedback checks cover packet validation, stop, legacy
mailbox compatibility, all six register-write failure positions, maximum
gain/duration, forwarding, nested calls and ABI mismatch rejection.

`tools/test-river-battery.py` still reproduces the historical 0267/0290/0291
defects. The new runner tests the current 0346–0348 applied source. Host
results establish software behavior; they do not prove motor output, charger
operation, calibrated charge percentage or panel luminance.

## Device validation sequence

1. Verify the candidate and full 0345 rollback hashes. Verify product River;
   use temporary `fastboot boot` only, never flash. Do not initialize audio.
2. Confirm fresh home/navigation/keyboard/Wi-Fi responsiveness before controls.
3. Collect the read-only `river-hardware-check` report, plus `battcheck`/IOPS
   publication. Compare voltage/current, raw blockers and published state.
   The check's default mode creates no haptic engine or player and performs
   no brightness/feedback action. Capability flags are not playback evidence.
4. With the user ready, check Control Center/Settings brightness, minimum and
   maximum brightness, repeated lock/wake, and UI responsiveness. Read back
   the framebuffer level; visually confirm actual luminance changes.
5. Exercise selection, impact and all three notification feedback patterns,
   user disable, keyboard and real system actions. Determine which private
   paths bypass the public hooks before extending them. Confirm a stop and
   continued navigation. Do not infer app-wide/Core Haptics support from a
   successful injected-helper pulse.
6. Collect timestamped battery/cable/charger state over transitions and a
   bounded load change before modifying the estimator or enabling charging.

The standalone library and `river-hardware-check` are built separately for
controlled native API tests. Stage and hash-verify them before using
`DYLD_INSERT_LIBRARIES=/usr/local/lib/libRiverHardware.dylib` with the helper.
Only explicit `--brightness 0..1` or `--feedback selection|impact|success|warning|error`
arguments request hardware actions. Default output includes native mode,
readiness, charger sample, selected IOKit properties, framework capabilities
and relevant runtime feedback-method signatures. Keep captures private.

Sources: the pinned Motorola/Qualcomm SMB5 definitions and status driver
linked in [BATTERY-STATUS.md](BATTERY-STATUS.md), Apple's
[UIKit feedback API](https://developer.apple.com/documentation/uikit/uifeedbackgenerator)
and [Core Haptics player contract](https://developer.apple.com/documentation/corehaptics/chhapticpatternplayer),
and the original reverse-engineered
[BackBoardServices setter declaration](https://github.com/davidmurray/ios-reversed-headers/blob/master/BackBoardServices/BackBoardServices.h).
The private BKS declaration is not independent proof of its current iOS16 ABI.
