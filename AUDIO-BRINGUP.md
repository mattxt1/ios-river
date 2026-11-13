# River iOS audio milestone

The next milestone is physical playback and microphone capture through iOS.
The current driver consumes output PCM and generates synthetic input. Neither
successful iOS API calls nor increasing frame counters prove physical audio.

**ADSP metadata live result:** the exact recovered stock MDT was accepted by
TrustZone PAS1 INIT_IMAGE (`status=0 result=0`), and iOS stayed responsive.
No executable firmware was loaded or started in that test. Patch0285 adds a
separate exact-carveout MEM_SETUP and bounded image staging step, with stock
input hashes verified before setup and at completion; it cannot start the DSP.
Destination-memory readback and full executable authentication remain unverified.

**PCM transport work:** stock ADSP firmware has been extracted read-only and
its embedded per-file SHA256 hashes verified. `tools/river-adsp-fw.py` checks
Hexagon metadata and exact River carveout bounds; this is not secure signature
authentication. Patch0282 adds an explicit read-only PAS1/retained-SMEM survey
(`river-adsp-status --probe`); the passive command reads its last result.
No ADSP firmware has been started, no physical PCM path is advertised, and
the hardware probe passed: PAS1 supported, init/mem/auth calls available,
valid retained SMEM, and no ADSP/APR channels in the checked legacy tables.
Those empty tables do not establish DSP power state. Patch0283 prepares an
exact-stock-MDT, one-shot INIT_IMAGE-only test with kernel-RAM/digest/crypto
guards; no executable load, DSP reset/start or power/clock vote. Its result is
pending. Subsequent work is source-derived secure startup,
SMP2P readiness, APR/QMI and Q6AFE/Q6ASM/SLIMbus buffer transport.

**Latest live result, 2026-11-12:** the muted codec-to-amplifier I2S clock test
passed on source `fa7283f` (through patch0281). Codec FLL locked (`raw=0x100`)
and amplifier raw PLL-unlock cleared twice (`0x20000010`→`0x2000000a`) with
mute, Class-D disable and slave mode verified. Codec/AIF, amplifier, PMIC
reference route and SPI restoration all passed. A usable clock path to the
amplifier is verified; clock frequencies/LRCLK waveform were not measured.
No PCM channels or analog outputs were enabled. Next is real PCM transport
and controlled data-path validation, followed by physical microphone capture.

**Verified on the phone, 2026-11-12:** CS47L35 identity `0x6360` on two SPI
reads, revision `0x1`, and restoration of all four SPI pin states. The explicit
probe returned success on the temporarily booted 0274 kernel. No codec register
was written. The amplifier still reports PLL unlock and no real PCM transport
exists. See NOTES.md for image/build hashes and exact readbacks.

**0276 live result:** FLL lock still times out, but FLL shutdown, complete clock
register restoration and SPI pin restoration all verified. PM8953 GPIO6 reads
as input mode 0; the Android overlay specifies DTEST3 clock output. Trace that
reference source before changing PMIC routing. Physical audio remains absent.

Patch 0277 adds `river-codec-clock --route-test`: a separately requested,
one-shot GPIO6 DTEST3 route test around the isolated FLL experiment. It applies
only the pinned board mux/pull configuration and restores the PMIC mode/pull,
all observed GPIO6 fields, codec clocks and SPI pins afterward. It refuses
missing APIDs or changed/non-idle state. The internal DTEST3 producer is left
unchanged and its availability is unknown; this is not a generic PMIC writer.
The source-derived values are mode `0x1c` and pull-disable `0x5`. Waits/retries
are bounded and cleanup failures are reported. No audio output is enabled.

**0277 live result:** enabling the pinned GPIO6 DTEST3 route allowed codec
FLL1 to lock (`raw=0x100`). The test exited successfully and verified PMIC
route, codec clock and SPI pin restoration afterward. This establishes a
usable reference path, without measuring frequency. The amplifier still
reports PLL unlock because its ASP clocks were not enabled; PCM input/output
remain placeholders. Next is a bounded AIF1 clock test with the amplifier
muted and verified in slave mode before codec clocks drive the shared lines.

Patch 0278 adds `--i2s-snapshot` and a one-shot `--i2s-test`. The test keeps
the CS35L41 muted with its Class-D output stage disabled and its ASP in slave
mode before the CS47L35 drives native AIF1 BCLK/LRCLK. It requests 48 kHz
stereo S16 I2S clocks (1.536 MHz BCLK), while TXDATA is tri-stated and PCM
channels stay disabled. Raw amplifier PLL-unlock must clear twice to pass.
Codec, PMIC and amplifier registers are restored afterward; a cleanup failure
retains protective amplifier mute/output-disable/slave settings. The volume
helper now uses the actual CS35L41 mute bit4 (`0x10`) and shares a control mutex
with the test so an iOS volume command cannot override its mute mid-test.
Run the snapshot before the test, and do not consume the shared one-shot with
`--test` or `--route-test` first. This is a clock-path test, not audible audio.

**0278 live baseline:** temporary boot succeeded, but `--i2s-snapshot`
repeatedly returned `phase=i2s_snapshot error=5 attempted=0` with SPI pins
restored. The amplifier survey retained expected ID and idle register values.
No clock test was triggered. Patch0279 reports the precise failing snapshot
stage/register/values without relaxing guards; on hardware it captured GPIO CTRL1 changing0xa001→0x2001 (only live
GP_LVL bit15). Patch0280 excludes that live bit from configuration comparisons
and explicitly selects native AIF1 mux from the observed GPIO-input defaults.
It still rejects active interfaces, GPIO outputs and other pin functions.
That comparison/mux correction is included in the later successful0281 run.

**0280 live attempt:** baseline succeeded and the one-shot test ran, but stopped
with `phase=amp_prepare error=5` before PMIC reference routing or I2S clocks.
Codec/AIF, amplifier, PMIC and SPI restoration all verified. iOS home screen
became responsive afterward. Patch0281 corrects SP_TX_WL to0x4830, uses the
PCM attenuation field correctly, and reports the first failed amplifier
write/readback/quiet check. Next attempt must wait for responsive iOS before
running the one-shot test. The corrected0281 run subsequently passed; the earlier
failed register was not captured, so its exact cause remains unproven.

## Current continuation

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
