# River iOS audio milestone

The next milestone is physical playback and microphone capture through iOS.
The current driver consumes output PCM and generates synthetic input. Neither
successful iOS API calls nor increasing frame counters prove physical audio.

## Verified progress and remaining work (2026-11-13)

- CS47L35 identity is verified as `0x6360`, revision1, over SPI7 at
  `0x07af7000`; all temporary SPI pin changes restored.
- The muted codec AIF1→CS35L41 clock test passed through patch0281. Codec
  FLL locked and amplifier PLL-unlock cleared twice. Codec, amplifier,
  PMIC reference route and SPI restoration verified; iOS stayed responsive.
  Requested clocks are48kHz stereo S16 /1.536MHz BCLK. Frequencies and
  LRCLK waveforms were not measured, and no PCM or analog path was enabled.
- TrustZone PAS1 accepted the exact stock ADSP metadata. Memory setup and
  the full17MiB firmware staging test then passed with matching input hashes.
  Physical destination readback and executable authentication remain unverified.
- Partition surveys found an empty APPS↔ADSP partition at SMEM+0x86000.
  Patch0288 then acquired/released remote lockS:3, initialized outbound
  SMP2P item429 and verified the complete block. An independent scan found
  one valid340-byte record at0x86030 and uncachedfree0x188, with no malformed
  records. No IPC, power vote or DSP start was requested. iOS stayed responsive.

The next audio step is correctly acknowledged RPM CX/XO votes with shared
ownership preserved, followed by authenticated PAS1 startup and remote SMP2P
negotiation/readiness. APR/QMI, Q6AFE/Q6ASM/SLIMbus PCM transport, codec routing
and physical microphone capture remain to be implemented. Output is currently
discarded and input is synthetic; endpoint counters and successful iOS API
calls establish only the existing software plumbing.

The Wi-Fi/battery audit fixes through patch0291 are committed in `236a880`.
Their host regressions and full kernel build passed. The packaged image
was accepted by temporary fastboot boot; responsive iOS and device validation
are pending.
See [WIFI-STATUS.md](WIFI-STATUS.md), [BATTERY-STATUS.md](BATTERY-STATUS.md)
and [NOTES.md](NOTES.md) for results and artifact hashes. Audio milestones
above were verified on earlier temporary images; they do not establish a
physical playback or capture milestone.

## Diagnostic implementation history

Patch 0273 adds `debug.river_audio_prereq`, a read-only, on-demand survey:

- Candidate PM8953 L6 and S4 observer PPIDs, and PMIC GPIO6 (MCLK pin)
  type/subtype, raw status and enable values. Failed reads remain explicit.
- TLMM GPIO46/68 and SPI7 GPIO135–138 configuration and input/output values.
- GCC BLSP2 AHB and QUP3 SPI clock branch state and source command register.
- No codec register writes, pinmux changes, rail votes, QUP7 controller access,
  or LPASS accesses. It uses the existing serialized SPMI observer helper,
  bounded/cached mappings and a mutex around concurrent survey calls.

Patch 0274 adds an explicitly requested, one-shot codec identity probe via
`river-codec-probe --probe`. The default invocation only reads the last result.
It requires exact observed L6/S4 type/subtype and ready/enable bits on two
successive checks, completed speaker/reset initialization, reset high, the
observed inactive QUP3 clock state, and all four SPI pins still idle inputs.
It temporarily drives CLK/CS/MOSI at low speed in SPI mode 0, reads only ID
register 0 twice and revision register 1 once, and restores/verifies pin
configuration and output latches. It does not turn on the QUP controller,
change codec registers, program supplies or touch LPASS. A failed prerequisite
leaves the pins alone. A transaction consumes the one attempt even on bad ID.
CS47L35 ID must equal `0x6360` on both reads; `0x6338` is CS47L85.

Patch 0275 adds `river-codec-clock --snapshot` (one clock snapshot per boot)
and `river-codec-clock --test` (one isolated FLL1 experiment per boot). The test
requires idle codec clocks and masked lock interrupts, configures only FLL
controls for a 32.768 kHz MCLK2 reference and requested 98.304 MHz output, waits
at most 25 ten-millisecond polls, disables the FLL and restores its registers
and the SPI pins. Readback or cleanup failures are errors. It never enables
SYSCLK/DSPCLK, AIFs or analog outputs. The passive command reads the last result.

Live 0275 testing did not achieve lock. FLL shutdown and GPIO restoration
verified, but the original clock-restoration comparison failed. Patch 0276
adds the codec 32 kHz enable from the full Cirrus initialization, explicit N
update-command rearming/clearing, precise mismatch diagnostics, repeated
read-only snapshots, and PMIC GPIO6 mode/voltage/pull/source readbacks. PMIC
clock routing is not changed. See NOTES.md for the current build/test state.

The fixed clock recipe is checked against both the pinned Motorola board/codec
source and [Cirrus's MCU driver at 82ba920b](https://github.com/CirrusLogic/mcu-drivers/tree/82ba920bace895e1a920aeaf0bb8a7df69c06c40/cs47l35).
That source documents the phase-control register and integer-mode gain used.
The board's `msm8952-dai-links.c` explicitly connects codec AIF1 to amplifier
PCM at 48 kHz stereo S16_LE, I2S, amplifier slave: 1.536 MHz bit clock. This
source route still needs physical clock/data validation.

A foreground app can be built with
`tools/build-river-audio-check.sh /absolute/output/directory`. The resulting
`RiverAudioCheck.tar` is accepted by the existing `appinstall --install` tool.
The app uses standard microphone consent, a quiet test tone, a three-second
capture with decoded frame/duration validation, and replay. A valid file can
still contain synthetic input; confirm physical sound separately.

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

The CLI recorder checks its own microphone permission before starting. On the
live diagnostic boot it was denied (`0x64656e79`), so a valid app capture still
needs a bundled iOS app with microphone permission granted through iOS. No
privacy setting is changed by these tools. A denied CLI capture does not prove
that an authorized app has a HAL failure.

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

1. Preserve the recorded prerequisite/transport readbacks and verify them on
   each new boot. Establish rail, clock and bus ownership before SPI control. Audit the
   existing IRQ GPIO46/63 configuration: patch 0271 writes `0x203`, which has
   TLMM output enable set; Android specifies IRQ inputs. Do not assume current
   pin labels or pinmux writes establish the board-level PCM route.
2. Identity and pin restoration have been verified. Implement bounded
   controller-based SPI and codec clock/control, with verified power/reset/clock
   prerequisites; preserve the default-off probe for future diagnosis.
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
