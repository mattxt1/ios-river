# River performance: first combined candidate

## 0341 observations and 0342 candidate

The user confirmed Spotlight blur works on image 0341. App-closing animation
was unchanged. Direct captures of compositor surfaces 2/4 show a black 132-pixel
status-bar strip and overlapping rounded shapes in Spotlight's search field.
The user also reported two corner radii during folder transitions. Spotlight's
process does not load the SpringBoard compatibility library, so its static field
and top strip cannot be attributed exclusively to that library.

Audio remained delayed; navigation continued to work. Six playback sessions
accepted/completed 2497536 frames; microphone input delivered 381952 frames with
zero timestamp gaps. Cleanup passed and the normal service remains enabled.
Backend timing from first non-silent PCM to first DMA submission was 1.354 s in
one sample and 5.226 s in an earlier sample. Coordinator ownership took tens of
microseconds; most delay occurred before codec readiness. These are backend
measurements, not UI-to-speaker or acoustic measurements.

Kernel patch 0341 addresses a source-level cause of avoidable scheduling:
`delay(5)` on every software-SPI edge can block the thread when the CPU's idle
latency is unknown. A codec-local absolute-time loop now retains the same
five-microsecond minimum edge dwell without scheduler sleep. It leaves global
kernel delays, interrupt/preemption state, PLL settling, amplifier leases,
register readback and cleanup unchanged. [Cirrus DS1085F2, table 3-20](https://statics.cirrus.com/pubs/proDatasheet/CS47L35_F2.pdf)
provides the SPI timing limits; retaining five microseconds does not raise the
software bus's existing maximum clock rate. Actual-source host tests cover
64 ordered mode-0 edges, identity decoding, invalid-register refusal and
stretched/preempted edges. Native build passed; device latency benefit remains
unverified.

Image 0342 combines that kernel with removal of fixed bitmap Search-pill and
folder plates in favor of native animated shapes. A bounded, default-off,
geometry-only trace can observe SpringBoard model/presentation opacity and
animation counts during app closure. It reads no text or image contents. The
static Spotlight field, black status-bar strip and app-closing cause remain
pending; this batch does not claim to fix all three. The verified 0341 candidate
image carries both its UI library and prior kernel and can restore that known
responsive state. Earlier images that omit the UI library cannot restore it.

## 0340 validation and next combined batch

The user confirmed clear Voice Memos playback and working home navigation and
keyboard after recording on image 0340, without a system UI restart. Microphone
input delivered 770048 frames with 385 exact READ completions and zero timestamp
gaps, invalid values or backwards values. Two playback sessions accepted and
completed 710656 frames; route/clock/pin cleanup passed. The service remains on.
Camera discovery returned normally in five later processes (0.61–0.76 seconds),
including after the app test; the initial five-second timeout remains unresolved.

The next requested batch targets fully transparent Spotlight, icons appearing
late after the app-closing animation, and about one second of delay for both UI
sounds and app playback. SpringBoard loads `/usr/local/lib/libcgprobe.dylib`.
Its original SHA256 is
`a8bd7ef11fc4d53423888c8914772973b35f4fbdc39d8e71fb70586af9f28b69`.
The library is backed up locally. Source includes explicit search/feather-blur
hiding and permanent overrides of rasterization/material snapshot state.
The candidate restores native search materials and those transition choices;
actual blur appearance and animation behavior require the combined boot test.
Unused image/glyph hooks remain uninstalled.

Kernel patch 0340 lowers the normal output startup threshold from 4096 frames
(85 ms at 48 kHz) to 1024 (21 ms). This removes up to 64 ms of buffer waiting;
it does not by itself explain or promise to eliminate a one-second delay.
Stage timestamps cover first non-silent PCM acceptance, coordinator ownership,
route entry, codec readiness, output enable and first DMA publication. They are
backend timings, not a microphone measurement of speaker latency or UI-to-sound
latency. Hardware settling checks, silence preroll, mute lease, ownership and
cleanup are preserved. Native kernel and UI library builds passed. Actual-source
coordinator/race, queue/volume/rearm and microphone pipeline regressions passed.

Image revision 0341 combines those changes, retaining renderer p0340 and the
camera-service configuration from image 0340. It carries the candidate UI library
at its existing load path. A separate UI rollback image must carry the verified
original library before candidate deployment: prior images do not carry this UI
library and therefore do not restore it by themselves. The UI rollback image
retains the new kernel; after it restores the library, prior images can again be
used for a full kernel fallback. No current library is overwritten during host
preparation. Boot images remain temporary; carried files persist on root as in
the established helper workflow.

The first screenshot helper selected a stale Settings surface. The display-default
surface helper returned a black unused surface. Neither capture verifies the
current Spotlight appearance. Keep the user's report as the visual baseline;
do not label those captures as a valid live screen.

## 0339 device results and follow-up

The fresh 0339 boot passed home/navigation/keyboard checks. During the cued
Settings scrolling sample, three complete renderer windows reported 33.5,
30.1 and 29.3 flushes/s; corresponding GPU-copy windows recorded zero drops.
These are throughput proxies. The user felt scrolling was about the same;
there is no healthy previous-build control establishing an improvement.
Background `aggregated` used 76% of one core during the 45-second CPU sample;
its sampled active stack includes PowerlogCore and SQLite. One stack does not
establish the cause of that sustained work. No daemon policy was changed.

Normal Voice Memos counting was clear, confirmed by the user. Microphone input
delivered 645120 real frames (13.44 s), with 324 exact DSP completions and zero
timestamp gaps/invalid/backwards values. Playback accepted and completed
543744 frames with zero queue drops/overflows/underruns. Cleanup passed.
The normal audio service remains enabled.

Afterward navigation/keyboard stalled again. SpringBoard's main thread was in
the same synchronous camera-discovery path. A single UI-only restart restored
navigation, confirmed by the user. The earlier scrolling sample precedes this
stall; subsequent unmarked samples cannot be treated as healthy measurements.

An isolated, five-second-bounded video-device discovery probe also timed out.
Bootstrap lookup found `com.apple.coremedia.capturesource` advertised, while
all three installed MediaToolbox recorder configurations have
`StartRecorderServer=false`. This supports a targeted availability experiment;
the exact causal endpoint still needs the changed-image test.

The follow-up candidate removes only that media-job service advertisement.
It leaves all audio services, all other jobs, the disabled recorder setting,
kernel 0339 and the working audio implementation intact. The repacker rejects
any unrelated launchd edit. Booting a prior verified image restores the original
configuration. First run the bounded discovery probe and verify it returns
promptly, then repeat normal Voice Memos and navigation in that same boot.

The follow-up renderer also labels detailed GL timing as unavailable when
profiling is off. The 0339 logger's zero GL timings and implied CoreAnimation
partition must not be interpreted as measurements. Coarse counters remain valid.

Patch 0339 has passed its native kernel build. Its paired Mesa renderer has
also built for iOS arm64. Hardware performance validation is pending. Preserve
the verified 0338 image and its original renderer as the fallback.

## What the stalled session showed

SpringBoard's main thread was waiting synchronously for camera discovery
through AVFoundation, CMCapture and CoreMedia. A single SpringBoard restart
restored navigation and the keyboard, confirmed by the user. This distinguishes
that UI stall from low rendering throughput; the specific unavailable endpoint
and a permanent fix remain unverified. Audio and backboardd were not restarted.

A roughly 24-second diagnostic sample also found substantial background CPU:
aggregated 67%, coreduetd 47%, biomed 39%, where 100% means one core. These
samples came from the degraded session. They establish investigation targets,
not normal idle costs, a proven cause, or an FPS baseline. No background service
was disabled or given a CPU limit.

## Changes combined in 0339

- Schedule framebuffer callbacks against fixed deadlines. Previously the next
  16.667 ms delay began after presentation work, extending each interval.
  Skip expired deadlines rather than scheduling catch-up bursts. Retain existing
  presentation, GPU fences and pending/show/retire surface ownership. Expose
  callback/skipped/max-lateness counters; `riverframeclock=0` restores the old
  timer behavior.
- Wake the normal audio coordinator on producer requests instead of polling
  every 2 ms while idle. Register the wait before rechecking the wake generation
  to prevent missed requests. Keep short-tail drain deadlines, active DSP
  behavior, mute lease and cleanup unchanged.
- Avoid diagnostic-only GL state queries when tracing is off. Make generated
  per-call timing and atomic accounting opt-in with `SWEETGL_PROFILE=1`.
  Keep coarse renderer timing and all real drawing/synchronization operations.

Tests execute the actual deadline helper, coordinator, queue, clear body and
generated dispatch wrappers. They cover work-induced timer drift, late/long
pauses, wake registration races, playback/capture alternation, stop/fault
handling, retained audio frames, trace gates, return values and profiling
on/off. Tests and successful builds do not establish a device FPS improvement.

## Reference projects and limits

[ios-sailfish](https://github.com/joranv1/ios-sailfish) reports CPU-bound rendering
and investigates presentation overhead. Its lower logical resolution and A530
hardware differ from River's iPhone X geometry and A506. Its clock settings and
rendering rates cannot be treated as River measurements.

[Apollo's frame-scheduling change](https://github.com/aTechGeekDev/ios-apollo/blob/1f75f381e31da96de537653fbac2252d2eb80418/xnu/patches/0148-apollo-fb-vsync-schedule-ddr-top.patch)
uses fixed deadlines and a separate presentation worker. River's first batch
adopts the smaller deadline change while preserving its established surface
ownership. Moving presentation to another worker remains a separate change
requiring ownership, display-power and queue validation.

[Apollo's CPU policy](https://github.com/aTechGeekDev/ios-apollo/blob/1f75f381e31da96de537653fbac2252d2eb80418/xnu/patches/0160-apollo-cpu-budget-touch-boost.patch)
is a useful reference for budgeting persistently hot background work. River
needs fresh healthy CPU samples and foreground/audio exemptions before adapting
such a policy. Broad process blocking and another platform's clock registers
are not included. [Apollo's Metal work](https://github.com/aTechGeekDev/ios-apollo/blob/1f75f381e31da96de537653fbac2252d2eb80418/userland/metal/SWEET.md)
targets A6xx and does not provide a ready A506 acceleration path.

## Fresh-boot validation

First verify home navigation, switching apps and keyboard entry. Treat a UI
stall as invalid context for scrolling measurements and capture its stacks
before recovery. After healthy startup settles, collect separate idle and
active CPU samples. Use consistent Home page swipes and Settings scrolling,
with phase boundaries recorded by the host. Read the existing renderer and
GPU-present logs along with deadline counters. Renderer flushes and GPU
presents are throughput proxies, not unique panel frames or frame-time
percentiles; exclude idle windows from scrolling comparisons.

Initialize the normal audio service once on the fresh boot, preserving its
existing muted setup. Check a normal app recording/replay and subsequent UI
navigation without manual per-app route changes. Preserve all safety leases.
Only compare against a fresh healthy reference running the previous image.
Report observed rates and responsiveness separately from build/test results.
