# River compared with current XNU/iOS ports

Research date: 19 November 2026. Scope: responsiveness, everyday app behavior,
graphics, audio, power, brightness and haptics. Telephony is excluded from the
proposed work. This is a comparison of published evidence, not independent testing
of other people's phones.

## Assessment

River is beyond initial bring-up, but it has not reached a consistently usable
system. Working hardware paths, successful builds and short responsive test
windows do not cancel the user's current report of frequent brief freezes.
Several other native ports are ahead in specific areas. There is no evidence
here that one of them has solved every part of an everyday phone.

The main gap is completion across layers: hardware, native framework, automatic
startup, normal app behavior, recovery and sustained use. River has often reached
the first layers while leaving later layers unfinished. Counting patches or boots
obscures that gap. Recent audio progress is real, but it does not improve scrolling,
app opening, late icons or unreliable system services by itself.

## River's actual baseline

Local sources: [README](README.md), [performance evidence](PERFORMANCE.md),
[hardware candidate](HARDWARE.md), and the newest [handoff](NOTES.md).

Last known device image is **0345**. Candidate **0349 has been built and host
tested but has not been booted**. This research made no device changes.

| Area | What we can credit | What still prevents normal use |
| --- | --- | --- |
| Boot, CPU, storage | Real XNU and iOS 16.7.16; four enabled cores, about 3.38 GiB usable RAM; persistent filesystem | More enabled cores cannot be assumed possible without checking this platform's translation and topology constraints |
| Graphics | Adreno 506/Freedreno EAGL path, GPU presentation, 60 Hz scanout | No established sustained 60 FPS result. Earlier scrolling windows were 33.5/30.1/29.3 renderer flushes per second, not independently counted displayed frames |
| Responsiveness | Many successful touch/navigation checks | User reports frequent brief freezes. Late home icons and app-closing behavior remain unresolved. Earlier main-thread camera discovery/XPC waiting demonstrates a separate failure class from GPU throughput |
| Audio | Stock Voice Memos physically records and replays recognizable speech, confirmed clear by the user | Once-per-boot muted setup and enable; automatic half-duplex routing, no qualified simultaneous record/play; startup reliability and endurance incomplete |
| Wi-Fi | Native Settings joins, DHCP/DNS/Safari and manual reconnect verified | Automatic association, lease renewal and endurance incomplete |
| Battery/charging | Physical voltage/current and estimated percentage | Learned SOC and measured temperature incomplete; charger ownership and policy incomplete. 0349 observes state but does not enable charging |
| Brightness | Physical DSI backlight control exists | AppleARMBacklight/CoreBrightness route had severe rendering side effects. New slider bridge and failure handling remain unbooted |
| Haptics | AW8691 identity and physical pulse established | New bounded UIKit bridge is SpringBoard-only and unbooted. This does not establish system-wide feedback, keyboard/app coverage or Core Haptics |
| Apps and peripherals | Stock apps/UI and browser use demonstrated | No comprehensive compatibility matrix; camera pipeline absent; Bluetooth not established; media, file providers and sensors need explicit qualification |

The README's sentence about Metal being translated through OpenGL ES should not
be treated as evidence of general Metal application support. The documented
running UI path is EAGL/Freedreno; a Metal API/compiler capability needs its own
device tests and process coverage.

## Main comparisons

Dates below identify the evidence inspected, not a claim that every repository's
latest commit was independently established. Repository counts, stars and inherited
fork history do not measure working features.

| Project / hardware | Published result and limitations | Comparison with River |
| --- | --- | --- |
| [HakiOS redwood][hakios], POCO X5 Pro, SD778G/A642L | November 3–5 measurements: native Metal window-server rendering; full-resolution page swipes 100–104 FPS, Settings scrolling 89–91. At 750×1624, swipes 107–118 and scrolling 118–120. Synthetic tests, frozen wallpaper and configuration choices matter; real-finger main-thread stalls remain [H1][H2] | Clearly ahead in measured graphics. Published device record also has Camera.app, audio, Wi-Fi and vibration; charging and Bluetooth remain incomplete [H3] |
| [UniXNU sheng][sheng], Xiaomi Pad 6S Pro, SD8 Gen2/A740 | Detailed pinned device record includes Metal, native Wi-Fi and Bluetooth evidence, audio and battery/charging integration. Audio quality, GPU recovery, frame stalls and some brightness/sleep behavior remain qualified or unfinished [U1] | Broader integration on substantially stronger hardware. Valuable framework and tracing reference; individual iPadOS/macOS results must not be merged into one universal pass |
| [ios-apollo][apollo], Mi 10T, SD865/A650 | Older baseline has GPU UI, Wi-Fi, Bluetooth/audio, brightness/fades and haptics. November 4–5 notes add experimental Metal, recurrent DeviceLost/black-screen failures, a flip-frame leak fix and an AVFlashlight framework hang fix. Next combined boot was untested [A1][A2] | Ahead in several system integrations, but latest development is also unstable. Battery work still fell back after probing the wrong gauge location; no credit for completed charging [A2] |
| [HakiOS Apollo][hakios-apollo], separate Mi 10T port | Device guide documents GPU UI, both microphones, speakers, brightness, vibration, Wi-Fi and battery; separate fuel-gauge and loader-assisted charging paths [HA1][HA2] | A distinct implementation, not the same project as ios-apollo. Useful audio/haptic framework reference; no assumption that loader charging equals a complete native charger policy |
| [Sailfish][sailfish], Pixel 1, SD821/A530 | October 22 baseline: GLES UI around 20–40 FPS; 750×1334 render scaled on GPU to 1080×1920; native brightness/fades, real battery and charging. Audio and Metal absent [S1] | Especially relevant A5xx graphics reference. Ahead on completed brightness/power behavior; River has physical app recording/playback that it lacks |
| [Spes][spes], Redmi Note 11, SD680/A610 | October 26: GPU UI, speaker, Wi-Fi and brightness. Vsync follows hardware frame counter. 90 Hz freezes UI/touch; opt-in Metal slow/glitchy; video crashes; mic untested; battery/charging placeholders [SP1] | Worth studying timing and brightness. It does not establish a smooth 60 FPS system or complete multimedia stack |
| [Bergamota][bergamota], Poco F1, SD845/A630 | October 25 release reports iOS 18 GPU UI around 45 FPS, Bluetooth and computer charging. October 26 notes move Chlorophyll to Turnip with shader/hang fixes. Default README still describes an older CPU-only state [B1][B2][B3] | Ahead on newer-userland/compiler and boot/power tooling; feature claims depend strongly on branch and release |
| [Flame][flame], Pixel 4, SD855/A640 | October 22: Metal2Vulkan/Turnip UI about 20–26 FPS, native Wi-Fi including boot join, stock apps and a third-party IPA launching/browsing. Audio/video/Bluetooth/real battery missing [F1] | Metal presence alone does not imply smoother UI. Useful app, networking and compiler integration reference; River ahead in physical audio |
| [Topaz][topaz], Redmi Note 12, SD685/A610 | Direct scanout, native Wi-Fi, gauge/charger work. README's newer introduction claims sound/charging/widgets/all cores, while older tables deny several of these [T1] | Important shared lineage, but contradictory status needs log/source qualification before using it as a feature target |

HakiOS's fast native path translates AIR through NIR/ir3 and records GPU work
directly; its alternate Metal2Vulkan path is a different implementation. Its
deferred command encoding and workload choices are useful architectural lessons,
not an A642L driver we can install on A506. General games and arbitrary Metal
applications are not established by the window-server benchmark [H2].

The HakiOS measurement log is particularly useful because it separates rendering
rate from input quality: it records touch-report delays and still identifies
100–300 ms SpringBoard main-thread turns. It also publishes changes that did not
help. Our comparison needs that same willingness to call an experiment unsuccessful
[H1].

UniXNU's device record gives another caution: disabling iconservicesagent improved
one median timing while removing application icons and worsening the tail. That
would fail River's usability goal even if an average benchmark improved [U1].

## Additional projects and filtering

- [ios-sweet](https://github.com/dipa-studio/ios-sweet): foundational lineage;
  inspected published baseline is much earlier and has software graphics/static
  battery. It does not represent the current performance ceiling.
- [ios-fog](https://github.com/MotoE7hype/ios-fog): honestly distinguishes inherited
  code from verified hardware. Touch contacts and inherited GPU/network features
  were not established in its published baseline. Not a mature alternative.
- [UMI](https://github.com/JopangJovan/hackios-xnu-kernel-umi) and
  [thyme](https://github.com/azzlantern/hackios-thyme): separate device overlays on
  the HakiOS family, useful for implementation discovery. Root README Redwood
  results must not automatically be credited to the Mi 10/Mi 10S.
- [Vince registry record](https://unixnu.com/devices/models/vince),
  [unixnu-vince](https://github.com/zjh3215588/unixnu-vince) and
  [ios-rebivincek](https://github.com/rebopawl-official/ios-rebivincek): relevant
  MSM8953-family discovery leads. Accessible evidence did not establish a mature
  device-tested feature set to reuse. A registry entry is not a boot result.
- [ASoftware](https://github.com/alpersamur3/asoft-ios-android): Android-hosted
  execution/virtualization research, including newer accelerated previews. Its
  published and preview configurations differ. It should be compared separately
  from River's native drivers; the linked S25 demonstration is not a released,
  reproducible River-compatible implementation.
- [Portsmith](https://github.com/Chromitsune/Portsmith-iOS) and the
  [UniXNU device index](https://unixnu.com/devices) helped discover projects.
  Their large inventories include early ports and inherited forks. They were not
  used as substitutes for each device's own evidence.
- The [devilAPI Sailfish fork](https://github.com/devilAPI/ios-sailfish) was checked
  alongside the original; shared README claims are not two independent results.

This is a broad survey of the relevant families and adjacent implementations,
not a claim to have exhaustively audited every registered device or all source
files. Several detailed notes and release records were substantially newer than
their repository home pages.

## Why River still feels experimental

1. **We have not closed the responsiveness problem.** A responsive check after
   a bounded hardware test is weaker than ten minutes of app switching, scrolling
   and keyboard use. Earlier synchronous camera-discovery waits are concrete
   evidence that a framework service can block interaction while the kernel and
   GPU remain alive. They do not prove the cause of today's every freeze.
2. **Automatic lifecycle remains incomplete.** Audio still needs host setup each
   boot; Wi-Fi startup/renewal and hardware framework behavior need qualification.
   Manual success is a development milestone, not a finished user feature.
3. **Our render workload and presentation policy need controlled evaluation.**
   A cropped high-resolution canvas can waste work. Lower logical resolution,
   GPU scaling and hardware timing are plausible experiments, not promised fixes.
   River already has GPU presentation; merely adding another GPU-copy patch is
   not an explanation of an upstream main-thread stall.
4. **Compatibility is tracked unevenly.** Late icons, file-provider behavior,
   media decoding and service dependencies can dominate ordinary use. Small UI
   hooks can improve an individual screen while leaving the blocking service
   underneath unchanged.
5. **We have accumulated candidate success without enough system acceptance.**
   0349's host checks protect implementation correctness; they do not establish
   brightness or haptic behavior on the actual phone. That distinction needs to
   stay visible in every status update.

## Recommended implementation order

These are proposed acceptance gates, not tests already passed or performance
promises. No telephony milestone is required.

| Order | Deliverable | Acceptance gate | References to study |
| --- | --- | --- | --- |
| 1 | Identify and fix recurring pauses | Repeat identical fresh-boot Settings/Safari/keyboard/app-switch flows; count stalls over 100/250 ms, capture wait stacks, correlate input/main-thread/commit/GPU fence/display times; compare both median and tail | HakiOS measurement/tracing design; Apollo wait/fence diagnostics; River's existing wait evidence |
| 2 | Make graphics consistently responsive | Compare current and lower-resolution profiles under the same scripted and real-finger workloads; count unique presented frames and frame intervals; preserve icons, animations and touch mapping | Sailfish A5xx scaling; Spes hardware frame timing; HakiOS deferred work; existing River presenter |
| 3 | Finish native brightness and power reporting | Normal slider, rapid changes, lock/wake and failed-transfer recovery; verify cable/load reporting; replace fake temperature; validate SOC and implement board-specific charger ownership/limits/policy | 0349 candidate; Sailfish brightness service; HakiOS Apollo gauge design; Bergamota power tooling |
| 4 | Remove routine host intervention | Audio and Wi-Fi initialize automatically; record/replay, app switching and reconnect work after a reboot; recovery does not need per-app control commands | Existing River half-duplex owner; lifecycle patterns from native HAL/network integrations |
| 5 | Complete system haptics | Confirm call paths for UIKit, keyboard, notifications and ordinary apps; preserve system preferences, cancellation and bounded actuator output; qualify Core Haptics separately | HakiOS actuator/HAL architecture and Apollo event patterns; 0349 endpoint is only the first layer |
| 6 | Expand useful app coverage | Published pass/fail table for Files/local storage, Photos/media/video, browser audio, notifications, time and sensor-dependent flows; repair one complete flow per candidate | Apollo extension/container work; Flame app/media results; Spes decoder failures |
| Later | Broader GPU API, Bluetooth, cameras and sensors | Independent end-to-end app tests, not device registration alone | HakiOS/UniXNU compiler architecture and device drivers after platform-specific audit |

For the first gate, collect a healthy baseline before enabling extra tracing.
Instrument only enough to identify the waiting stage; logging and sampling can
themselves disturb timing. Include temperature, clocks and background CPU in the
record, but do not infer a causal fix from one hot or degraded session.

The immediate engineering objective should be **a repeatably responsive normal
iOS session with fewer manual prerequisites**. More low-level features should
enter that session through complete user flows. A Metal rewrite on A506 is a
separate substantial project and should not postpone fixing measured waits.

## Source ledger

- [H1: HakiOS measurements](https://github.com/oosexgang/hakios-xnu-kernel/blob/main/docs/reference/metal-measurements.md), November 3–5 runs.
- [H2: HakiOS Metal implementation and limitations](https://github.com/oosexgang/hakios-xnu-kernel/blob/main/docs/reference/metal.md).
- [H3: Redwood device record](https://github.com/oosexgang/hakios-xnu-kernel/blob/main/docs/devices/redwood.md).
- [U1: sheng device evidence][sheng], generated from record `c8f92b208068eeaa4cd2b0f592671f04ee70076e`.
- [A1: Apollo README](https://github.com/aTechGeekDev/ios-apollo/blob/main/README.md).
- [A2: Apollo notes](https://github.com/aTechGeekDev/ios-apollo/blob/main/NOTES.md), latest visible November 5, commit `1f75f38`; [history](https://github.com/aTechGeekDev/ios-apollo/commits/main/). Read through the rendered browser because text retrieval omitted the large file. Combined boot 125 remained pending in that record.
- [HA1: HakiOS Apollo repository][hakios-apollo]; [HA2: Mi 10T guide](https://github.com/enesuzun2002/hakios-apollo/blob/apollo/devices/mi-10t/README.md).
- [S1: Sailfish README][sailfish], October 22 status.
- [SP1: Spes README][spes], October 26 status; its older lower section is not fully reconciled with the latest table.
- [B1: Bergamota releases](https://github.com/ry-diffusion/Bergamota/releases), October 25 Chlorophyll milestone; [B2: develop notes](https://github.com/ry-diffusion/Bergamota/blob/develop/NOTES.md), October 26; [B3: README][bergamota], older feature table.
- [F1: Flame README][flame], October 22 status.
- [T1: Topaz README][topaz], conflicting October 20/22 introduction and older table.

No third-party code was imported, no performance fix was claimed, and no device
was rebooted or configured for this comparison. Transferring code later requires
checking its actual license and the relevant hardware contract per file.

[hakios]: https://github.com/oosexgang/hakios-xnu-kernel
[sheng]: https://unixnu.com/devices/sheng
[apollo]: https://github.com/aTechGeekDev/ios-apollo
[hakios-apollo]: https://github.com/enesuzun2002/hakios-apollo
[sailfish]: https://github.com/joranv1/ios-sailfish
[spes]: https://github.com/AdyaGMD/ios-spes
[bergamota]: https://github.com/ry-diffusion/Bergamota
[flame]: https://github.com/NWylynko/ios-flame
[topaz]: https://github.com/MakrSas/ios-topaz/blob/topaz/README.md
