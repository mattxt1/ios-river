# River iOS audio milestone

The next milestone is physical playback and microphone capture through iOS.
The current driver consumes output PCM and generates synthetic input. Neither
successful iOS API calls nor increasing frame counters prove physical audio.

## Current continuation

Patch 0273 adds `debug.river_audio_prereq`, a read-only, on-demand survey:

- Candidate PM8953 L6 and S4 observer PPIDs, and PMIC GPIO6 (MCLK pin)
  type/subtype, raw status and enable values. Failed reads remain explicit.
- TLMM GPIO46/68 and SPI7 GPIO135–138 configuration and input/output values.
- GCC BLSP2 AHB and QUP3 SPI clock branch state and source command register.
- No codec register writes, pinmux changes, rail votes, QUP7 controller access,
  or LPASS accesses. It uses the existing serialized SPMI observer helper,
  bounded/cached mappings and a mutex around concurrent survey calls.

`HAKIOS_AUDIO_GET_STATUS` is a versioned ioctl on both PCM endpoints. It reports
software frame counters and the current `PLAYBACK_DISCARD` and
`CAPTURE_SYNTHETIC` flags. It does not advertise a real transport. Misaligned
PCM I/O fails with `EINVAL` instead of returning success without progress.

The raw test refuses physical testing when status is absent or the relevant
backend is a placeholder. `--plumbing` explicitly permits that case. It retries
short writes and interrupted I/O, fails on incomplete reads, and optionally
saves mono 48 kHz 16-bit WAV without overwriting existing files. Failed captures
are removed. The iOS recorder now services its completion delegate, checks
session/recording failures, decodes the resulting file and checks its duration.

```sh
/usr/local/bin/river-audio-survey
/usr/local/bin/river-audio-test status
# These must refuse the current placeholder backend:
/usr/local/bin/river-audio-test play 440 1 0.05
/usr/local/bin/river-audio-test record 1 /tmp/physical-mic.wav
# Explicitly test software plumbing instead:
/usr/local/bin/river-audio-test record 1 /tmp/plumbing-mic.wav --plumbing
/usr/local/bin/river-record-sound 2 /tmp/ios-capture-new.caf
```

Run host fault-injection tests with `python3 tools/test-river-audio.py`.
They exercise backend gating, unknown ABI versions, EINTR, partial transfers,
zero writes, EOF, odd-byte reads, file preservation and WAV lengths. The survey
is compiled from the actual patch with substituted MMIO/SPMI helpers to check
its access boundary, failed mappings/reads, unsupported controllers and repeated
polls. These tests do not validate physical hardware.

## Source evidence and remaining implementation

The Motorola source is pinned to
[`2dba45b8`](https://github.com/LineageOS/android_kernel_motorola_sdm632/tree/2dba45b8e247958d5a1b15b33c6064c15af4983e).
Relevant paths under that revision:

- `arch/arm64/boot/dts/qcom/sdm632-river-audio-overlay.dtsi`: L6 supplies AVDD,
  DBVDD and CPVDD; S4 supplies DCVDD. Codec SPI7 reset GPIO68 and IRQ46.
  PMIC GPIO6 is configured as dtest3 for MCLK. Madera SLIMbus is enabled.
- `sdm632-moto-common-overlay.dtsi` and `sdm632-moto-pinctrl-overlay.dtsi` in
  that directory: BLSP2 QUP3 SPI7 at `0x07af7000`, interface/core clocks, and
  GPIO135 CLK, 136 CS, 137 MOSI, 138 MISO.
- `include/dt-bindings/clock/msm-clocks-hwio-8953.h`: GCC branch offsets
  `0x0b008` and `0x0f01c`; QUP3 SPI source command `0x0f024`.
- `drivers/mfd/madera-core.c`, `madera-spi.c`, `cs47l35-tables.c`: codec supply,
  reset, identity and boot sequencing; 32-bit SPI address plus 16 pad bits.
- `sound/soc/codecs/madera-slimbus.c`, `techpack/audio/asoc/msm8952-slimbus.c`,
  `drivers/slimbus/slim-msm-ngd.c`: PCM data path and ADSP dependency. Codec
  control over SPI alone does not implement audio transport.

L6 `SID1:0x4500` and S4 `SID1:0x1d00` are candidate addresses from the Qualcomm
PMIC layout (matching the existing L5/L7 and S5 surveys), not measured voltages.
A readable status/enable value cannot establish rail voltage or board ownership.
Confirm identity/subtype and regulator configuration before changing supplies.

The remaining sequence is:

1. Boot the diagnostic image and save the prerequisite/transport readbacks.
   Establish rail, clock and bus ownership before any SPI transfer. Audit the
   existing IRQ GPIO46/63 configuration: patch 0271 writes `0x203`, which has
   TLMM output enable set; Android specifies IRQ inputs. Do not assume current
   pin labels or pinmux writes establish the board-level PCM route.
2. Implement bounded codec identity/control transfers with correct SPI framing
   and verified power/reset/clock prerequisites. Verify the actual CS47L35 ID.
3. Establish ADSP firmware availability, secure loading/ownership, IPC and
   SLIMbus/Q6AFE capture/playback. Do not repeat direct LPASS MMIO accesses;
   they aborted in r153. A replacement AP transport needs its own verified
   clock, routing, DMA and access-control design.
4. Connect real buffers to the existing iOS HAL with pacing, bounded buffering,
   underrun/overrun handling, lifecycle, mute and volume. Remove synthetic input
   only once physical capture is available, and update backend status honestly.
5. Validate audible low-level output, recording of an identifiable changing
   external sound, replay through AVFoundation, simultaneous input/output,
   mute/volume, sleep/wake and concurrent haptics. Keep recorded artifacts and
   hardware logs outside Git.

Temporary fastboot boot only. No partition flashing is needed for this work.
See the active entry in NOTES.md for exact build and live test results.
