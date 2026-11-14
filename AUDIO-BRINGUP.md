# River iOS audio milestone

The next milestone is physical playback and microphone capture through iOS.
The current driver consumes output PCM and generates synthetic input. Neither
successful iOS API calls nor increasing frame counters prove physical audio.

##0314 codec channel lifetime verified

Sink0/channel144 connection, mono48k16-bit activation/reconfiguration,
removal and disconnection all received matching success ACKs. All channel
ownership flags cleared, no uncertainty; user confirmed responsive iOS.
PCM/QMI/enumeration passed again. Codec SLIMRX-to-AIF routing and physical
sample output remain pending, with amplifier mute retained.

##0314 codec-channel lifetime candidate ready

The verified next image batches codec sink0/shared channel144 connection,
48k16-bit activation/reconfiguration, removal and disconnection with exact
ACK ownership. Host sanitizer tests and native build pass; hardware result
pending. No PCM source or codec/AIF writes are added by this test.

##0313 capability exchange and Madera logical address verified

The polled controller exchanged capabilities and queried the exact codec
address successfully: Madera logical0x8a. No DMA, IRQ enable or data-channel
routing. PCM/QMI passed again; user confirmed responsive iOS. Next is codec
sink/channel activation and the remaining codec/AIF physical route.

##0313 controller capability/codec address candidate ready

The verified next image batches polled NGD enable, master capability,
satellite report and exact Madera logical-address query after the passing
PCM/QMI/powered readback stages. Host tests and final kernel/helper builds
pass. IRQ and message DMA stay disabled, amplifier muted; hardware result
pending. Physical playback/capture remain pending.

##0312 SLIMbus service, QMI power and hardware version verified

PCM passed again. SLIMbus service node5/port3, QMI instance0 selection and
power ACTIVE all passed; powered NGD version0x01050000 readback succeeded.
Controller remains disabled and amp muted. User confirmed responsive iOS.
Next is capability exchange and codec logical address before physical routing.

##0312 QRTR endpoint and powered controller snapshot candidate ready

0310 exposed valid HELLO control ports0xfffffffe, matching QRTR. The new
verified image corrects both transmit/receive endpoints and batches read-only
NGD version/status registers after successful QMI selection/power only.
Host tests and native builds pass; hardware test pending. PCM consumption
is repeatedly verified, but physical playback/capture remain pending.

##0310 broadcast reply correction temporarily booted

0309 PCM passed again, then local validation rejected DSP node5's broadcast
HELLO and closed CID2 before QMI. iOS remained responsive.0310 accepts router
broadcast controls, retains strict QMI endpoints and adds reply metadata.
Host tests and kernel build pass; corrected image temporarily booted.
Responsive-home confirmation and hardware tests are pending.

##0309 SLIMbus control candidate ready

The next verified image batches repeat PCM validation with CID2 IPCRTR
HELLO/service discovery, then SLIMbus QMI select-instance and power requests.
Host tests and kernel/helper builds pass. No0309 hardware result yet.
No SLIMbus MMIO or codec routing is attempted by this candidate.

##0308 zero-PCM completion verified with responsive iOS

The DSP consumed four mapped4096byte zero-filled buffers with exact successful
WRITE_DONE checks. All stream/device/AFE resources closed and mapping was
released; permanent memory and hardware mute are retained until reboot.
User confirmed responsive iOS afterward. Physical SLIMbus/Madera stream,
audible playback and microphone capture remain pending. iOS /dev backend
still discards output and synthesizes capture.

##0306 second-start rejection;0308 combined candidate ready

0306 repeated all prerequisite/routing tests, then its second AFE start
returned DSP0x9. No PCM was submitted; mapping and all routes cleaned up,
with hardware mute retained.0308 combines hot routing and zero-PCM completion
in the first start after cold route verification, and rechecks exact config.
Host sanitizer tests and incremental kernel build pass; hardware test pending.

##0306 muted PCM DMA/completion candidate ready

Source98dcdb1 and the verified temporary image add four zero-only mapped
PCM writes after ASM RUN, exact WRITE_DONE ownership checks, and bounded
teardown. Host sanitizer tests and native kernel/helper builds pass.
Hardware validation is pending.0305 remained responsive after all routes.
The amplifier stays muted; physical playback and capture remain pending.

## AFE readback, ADM routing and AFE start/stop verified on0305

The0305 boot verified exact mono48k/16-bit/channel144 AFE configuration,
ADM COPP0 and RX matrix routing, then a muted AFE start/route/stop cycle.
Both ASM/COPP sessions closed, AFE stopped, rings were consumed and no
uncertain ownership remained. The amplifier stays muted until reboot.
No ASM RUN/WRITE/sample DMA, physical playback or microphone capture has run.
Completion-driven PCM buffers are the next implementation stage.

## AFE SET accepted; readback correction0305 ready

0304 repeated PCM map/unmap and ASM format/close successfully. AFE accepted
SET and returned GET status0, but the local checker rejected its larger
response; no ADM or AFE start followed.0305 corrects GET capacity and parses
the declared record with explicit config diagnostics. Tests and incremental
kernel compile/link/install pass; image is verified and ready for a fresh boot.
Physical playback/capture remain unimplemented. See active NOTES.md.

## AFE/ADM candidate0304 built and temporarily booted

0303 hardware mapping/session prerequisites passed.0304 batches AFE port
configuration, inactive ADM RX matrix mapping/teardown, and a muted AFE
start/routing/stop sequence. Host sanitizer/regression tests and the recovered kernel compile/link/install
passed. The verified image temporarily booted; home confirmation and actual
AFE/ADM device tests are pending. Amplifier mute is retained until reboot
for the hardware-facing test. No sample streaming/capture backend is added.

## Shared PCM map/unmap and ASM setup verified on0303

The responsive0303 boot passed exact retained16KiB-page ASM mapping and
matching-handle unmap, followed by session1/stream1 open, signed stereo48kHz
16-bit format, and close. The in-band AFE SLIMbus0RX GET received a matched
DSP status0x2 rejection; the channel stayed healthy, rings consumed, and
amplifier baseline unchanged. User confirmed iOS remained responsive.
No physical sample submission, ADM route, AFE port start, SLIMbus activation
or physical microphone capture has run. See active NOTES.md for exact evidence.

## Attenuation correction verified (2026-11-13)

Patch0293 fixes the runtime CS35L41 volume helper before physical playback.
The iOS ABI supplies positive attenuation in eighths of a dB. PCM_VOL is a
signed11-bit field in bits13:3:80 means -10dB, encoded0x7b0<<3 =0x3d80.
The old helper wrote raw80, placing positive10 in that field. Patch0293
preserves unrelated/ramp bits, clamps attenuation to the existing ABI90dB
limit and uses dedicated mute code0x4cf<<3. It holds output mute during
preparation and verifies the level before clearing mute; failed preparatory
I2C writes/readbacks retain mute. A failed final mute-state read requests
mute again. The helper retains its existing void interface; it does not yet
propagate a hardware error through the volume ioctl.

The [Cirrus driver](https://github.com/torvalds/linux/blob/v6.1/sound/soc/codecs/cs35l41.c#L328)
uses signed PCM volume at shift3 with mute raw0x4cf. Host tests extract the
actual baseline and patched helper: baseline wrong-encoding and failed-write
unmute reproduce; corrected signed levels, preserved fields, range clamp,
explicit mute, unavailable speaker and injected read/write/readback failures
pass. Live4d238b8 register tests now pass:80 eighths below0dB→0x3d80,720→0x2980,
explicit mute→0x2678/outputmute0x10. Baseline digital0/outputmute0, format,
PLL and power registers restored exactly. No PCM was played and no codec
clock was changed. Wi-Fi regressions and the
full a6de705 kernel build pass with0292/0293 together. Device register verification passed; the image also includes the RPM reply fix below.

## RPM reply prerequisite (2026-11-13)

Patch0294 corrects the boot RPM reply receiver. Live236a880 USB logs showed
ACK1 consumed for both L19 request1 and L9 request2. The old code accepted
any received packet as success. The new receiver requires the exact current
nonzero message ID, V0 service, declared payload length and msg# key/length.
It consumes valid stale replies without accepting them, identifies matched
NACKs, waits for complete frames, handles FIFO wrap and rejects malformed
lengths/indices/states without advancing the RX tail. Error payload text is
never logged. Polling remains bounded; stale logs are capped.

Actual receiver host tests pass under AddressSanitizer/UndefinedBehaviorSanitizer
for duplicateACK1→request2, matched/stale NACKs, partial/wrapped/malformed
frames, invalid states/indices and restricted RX-tail/flag/IPC writes.
Primary V0 layout: [matching Motorola RPM driver](https://github.com/LineageOS/android_kernel_motorola_sdm632/blob/2dba45b8e247958d5a1b15b33c6064c15af4983e/drivers/soc/qcom/rpm-smd.c).
Sequential0292→0293→0294 application to the exact236a880 source passed.

Patch0294 passed live on bb395d6: exact ACK1, stale ACK1 ignored while
awaiting2, then exact ACK2. Patch0298 now refactors the existing L19/L9 boot
requests into a shared, mutex-owned transport. It validates the observed SMEM
item18/342 layout, channel states and FIFO indices; reserves four ring bytes;
waits at most500ms for TX space and500ms for the exact reply; publishes packet
contents before the head/doorbell; and never reuses a request ID during the
boot. IDs begin at0x52490001 to distinguish them from inherited low IDs.
Future audio clients are gated until both boot requests receive exact ACKs.
This readiness ledger is not a record of CX/XO ownership or a power rollback.
A timeout after sending means the remote outcome is unknown.

Actual-code sanitizer tests pass for exact packet bytes, one/two/three KVPs,
wrap, full/exact-capacity rings, delayed drain/replies, stale/NACK/timeout,
invalid layouts/states/indices, ID exhaustion and the audio readiness gate.
The full678e917 kernel build passed. Its temporary image packaged with all
kernel segments verified (26,697,728 bytes,577 command-line bytes,1MiB
retained log). Temporary fastboot boot returned OKAY. Live USB diagnostics
verify exact ACKs for IDs1380515841/1380515842 and shared transport ready1
with boot ACK mask3. The user confirmed a responsive iOS home screen; the
read-only audio baseline and PAS/empty-partition survey passed on this boot. It issues the
same existing L19/L9 votes and adds no ADSP votes or DSP startup. WCNSS startup
still follows its existing policy even when boot votes fail. CX/XO shared
ownership policy and authenticated startup were subsequently implemented and
verified by0299 below.

## Authenticated startup verified (patch0299,2026-11-13)

The explicit `river-adsp-status --start` operation requires accepted stock
metadata, a completed hash-verified17MiB stage, the owned outbound429 record,
no existing ADSP legacy channels or inbound443 record, PAS start/stop support,
known enabled crypto clocks and the shared RPM ready ledger. It rechecks
ownership and crypto state immediately before PAS1 AUTH_AND_RESET. It is not
invoked automatically at boot. `--start-status` reads the separate outcome.

The experimental power policy retains active and sleep CX at the board-normal
TURBO level384 with enable1 and stock100mA proxy load, and shared XO enabled1.
CX is RPM `smpa` resource2 with `vlvl`, not raw microvolts; XO is `clk0`
resource0 with `Enab`. All four transactions require exact ACKs under the
shared transport mutex. The persistent policy intentionally adopts those
APPS requirements for the rest of the boot. It does not recover the inherited
vote, implement dynamic consumer aggregation or claim rollback. Existing
unrelated keys are left alone. A failed or partially acknowledged sequence
retains its attempted votes, reports attempted/ACK masks and cannot be retried
that boot. No unvote-to-zero or invented baseline restoration is performed.
This can increase power use and constrains suspend; thermal/suspend behavior
is unvalidated. Earlier `vfl` floor-key ideas were not implemented.

After secure authentication succeeds, startup sends bounded SMP2P doorbells
(bit10) and reads the observed uncached APPS↔ADSP partition. It validates
allocation bounds, unique records, version/PIDs, stable headers and the exact
`slave-kernel` entry. Our supported feature subset is0; readiness requires the
remote to converge to version1/features0 and set ready bit1 without fatal
bit0. Polling is limited to500×20ms and at most four doorbells. This does not
bound the synchronous secure-monitor calls themselves. A failed auth, fatal,
malformed snapshot or readiness timeout triggers one PAS1 shutdown attempt;
its actual status/result is reported. Shared power remains retained. A ready
result is one observed startup state, not continuous liveness monitoring.

Actual-code ASan/UBSan tests pass for all prerequisite/clock/ownership gates,
the exact active/sleep power plan, every vote failure/timeout, pre-auth
rechecks, authentication and shutdown errors, delayed feature negotiation and
readiness, fatal-before-ready, bounded timeout, allocation/header races and
malformed SMEM. The updated iOS helper compiles with Wall/Wextra/Werror and
is signed ad hoc with its existing privilege model. Patch application passes.
Full6ac8fd5 kernel build passed, with no numbered project patches skipped.
The verified temporary image is26,716,160 bytes with577 command-line bytes
and1MiB retained log; all kernel segments verify against the new kernel.
The user confirmed a responsive home screen on its temporary boot. Fresh
metadata acceptance,17MiB staging and locked handshake initialization passed.
The one-shot start then passed live: all four exact power ACKs (mask0xf),
PAS1 AUTH_AND_RESET status0/result0, remote version1/features0 and
`slave-kernel=0x6` (ready plus proxy-unvote), after3polls and1interrupt.
The documented CX/XO policy retains those requirements; it does not honor
proxy-unvote yet. No shutdown was requested. Post-start SSH and read-only
surveys worked, and the user confirmed the iOS home screen remained
responsive after startup.

Post-start private SMEM contains17valid/stable records, including a2048-byte
channel table (item13) at0x86300. The current global-only channel count remains
zero and cannot describe that private table. APR transport discovery is next.
Amplifier baseline registers remain unchanged and flags3 still identify
synthetic capture/discarded playback. This is DSP startup evidence, not
physical audio. Local artifact hashes remain in
`outputs/adsp-start-build-manifest.json`; captures stay outside Git.

The stock driver uses PAS1, an80MHz crypto source and the same CX/XO proxy
requirements. Its XO branch is shared by LPASS, USB and WLAN; it assumes
clocks enabled at handoff and does not recover inherited APPS votes. Source:
pinned Motorola `msm8953.dtsi`, `msm8953-regulator.dtsi`,
`pm8953-rpm-regulator.dtsi`, `rpm-smd-regulator.c`, `clock-gcc-8953.c`,
`clock-rpm.c`, `msm8953-smp2p.dtsi`, `msm_smp2p.c` and `subsys-pil-tz.c`,
revision2dba45b8e247958d5a1b15b33c6064c15af4983e.

## Private transport discovery verified (patch0300)

Live startup created a private item13 channel table that the existing global
scan cannot inspect. Patch0300 adds bounded read-only private SMD table
metadata,40/88-byte control layouts and FIFO index reporting, plus GLINK
items478–480 and descriptor indices. It reports invalid/duplicate records,
changing snapshots, invalid names/CIDs and truncated output. It does not
allocate, open a channel, consume ring bytes or send IPC. Table266 bridge
names are reported without assuming table13 control layout.

Actual-source sanitizer tests pass for the two SMD layouts, GLINK indices,
padding/unknown/duplicate/malformed allocations, concurrent publication,
invalid names/CIDs, bounded records/output and zero shared-memory writes.
The full8278313 kernel build and image verification pass, with no numbered
patches skipped. The temporary image is26,716,160 bytes,577 command-line
bytes and retains1MiB of kernel log. Live validation passed on8278313: startup again reached ready0x6, then the
passive survey found7valid private SMD channels and no GLINK478–480 records.
`apr_audio_svc` is table13 slot6, CID6, flags0x201, refs1, with40-byte controls
at0x9ac78 and16KiB FIFO at0x9acb0. Host state0/peer1 and all ring indices0.
The user confirmed responsive iOS afterward. No ring bytes were consumed or
channel opened. Other private names are LOOPBACK, IPCRTR, DIAG_CNTL, DIAG,
fastrpcsmd-apps-dsp and apr_apps2. The amplifier baseline stayed unchanged.

The pinned source points to SMD: River's configuration enables SDM450 and
disables core GLINK; the audio build includes `sdm450auto.conf`, which selects
`CONFIG_MSM_QDSP6_APRV2`. `ipc/Kbuild` maps that option to `apr_tal.c`, whose
ADSP audio channel is `apr_audio_svc`. That source configuration now matches live discovery. A bounded APR CORE
GET_STATE request (0x1290c, response0x1290d) is the next transport check before
AFE/ASM buffer and codec routing work. No APR request has been sent yet.
Sources: [River configuration](https://github.com/LineageOS/android_kernel_motorola_sdm632/blob/2dba45b8e247958d5a1b15b33c6064c15af4983e/arch/arm64/configs/river_defconfig),
[audio configuration](https://github.com/LineageOS/android_kernel_motorola_sdm632/blob/2dba45b8e247958d5a1b15b33c6064c15af4983e/techpack/audio/config/sdm450auto.conf),
[APR SMD transport](https://github.com/LineageOS/android_kernel_motorola_sdm632/blob/2dba45b8e247958d5a1b15b33c6064c15af4983e/techpack/audio/ipc/apr_tal.c),
[CORE query](https://github.com/LineageOS/android_kernel_motorola_sdm632/blob/2dba45b8e247958d5a1b15b33c6064c15af4983e/techpack/audio/dsp/q6core.c).


## APR CORE query: live request consumed, reply unresolved (patch0301)

The explicit `--apr` operation validates the live ready DSP and the observed
CID6/flags0x201/40-byte/16KiB private SMD layout. It refuses existing ownership
or nonempty rings, rechecks after mapping IPC, follows byte-flag OPENING→OPENED
and sends one APR V0 CORE GET_STATE with a distinct nonzero token. Responses
must match addresses, ports, token, opcode and lengths. Basic success ACKs
are not mistaken for a state response; stale packets are counted, errors are
reported, malformed framing remains unconsumed. TX reserves4 bytes and all
memory access/consumption is bounded. Peer-owned indices are never reset.

Opening waits at most500×10ms, then a matching stock200ms service settle;
TX space100×5ms and response200×5ms. Success retains the owned OPENED channel
without an asynchronous RX worker; it is a one-time transport check, not
continuous liveness or PCM service. Failure requests protocol CLOSED and
waits100×5ms for peer/flag acknowledgment, reporting any unverified cleanup.
No retry is allowed after claiming ownership that boot. Shared power policy
is unchanged. No AFE/ASM/SLIMbus commands, sample-buffer mapping or audio
output/capture paths are enabled. `--apr-status` reads the separate outcome.

Actual-code sanitizer tests pass for owned discovery and rechecks, exact
wire command/byte flags, stale/basic/NACK replies, malformed/partial/wrapped
frames, index races, wait bounds and close failures. The iOS helper compiles
with Wall/Wextra/Werror and normal ad-hoc signing; patch application passes.
Full3e9f5cd kernel build and packaging passed; no numbered patches skipped,
all kernel segments verified. Temporary image size26,732,544 bytes with577
command-line bytes and1MiB retained log. Fastboot accepted its verified
temporary boot. Its live test is recorded below;
artifact hashes are in `outputs/adsp-apr-build-manifest.json`.

Live test on3e9f5cd opened SMD in2polls and sent40bytes, which the DSP
consumed (TXhead=tail=0x28).56unclassified bytes appeared in RX; the operation
stopped with EIO at RXpoll2 without consuming them. Close was requested;
post-state is host CLOSED0/peer CLOSING4 and RXhead0x38/tail0. The user
confirmed responsive iOS; amplifier baseline is unchanged. No retry was run.

The old status lacks both the received header and the last live ready-gate
snapshot. EIO can originate from either gate or parser; historical startup
ready0x6 does not establish post-query readiness. Queue size alone does not
identify the response format. A valid CORE response has not been verified.


## Declared response framing verified (patch0302)

The pinned APR receiver accepts V0/V1, a declared common-prefix header of at
least20bytes and payload sized by the APR declaration. Q6 CORE accepts at
least one state word.0301's fixed20-byte header/exact24-byte state requirement
was narrower.0302 supports bounded declared headers and APR size within the
complete SMD packet, with minimum4-byte state/8-byte basic-result payloads.
Exact service/domain/ports/token/opcode checks remain. Response/event types
are supported for the known response opcode; command types are not accepted.

The status now records SMD length, common APR header metadata and the live
SMP2P snapshot used by the ready gate. It does not dump raw payloads. Close
reporting recognizes peer CLOSING4 with host CLOSED as the stock closing-list
handler does, and reports that peer state without claiming empty/restored
rings. No peer-owned index reset or retry is introduced.

Actual-source ASan/UBSan tests reproduce0301's fixed-framing/CLOSING limits
and pass0302's extensions, padding/length bounds, address/token checks,
readiness failure with queued RX, partial/wrap/races, errors and close/no-ACK.
The device's queued bytes have not been recovered; compatibility fixtures
do not prove their format or the actual failure cause. Full1334e8c build and image verification pass; temporary fastboot boot returned
OKAY. The home-screen confirmation, firmware/startup sequence and live0302 APR
validation passed. Artifact hashes: `outputs/apr-framing-build-manifest.json`.

## Bidirectional APR milestone (1334e8c)

The live CORE query passed: exact source/destination/token/opcode match,
returned DSP state1, TX40bytes consumed and RX56bytes consumed. The actual
SMD body is36bytes; APR header0x00240081 declares version1 and32-byte header,
with state at the declared payload offset. This validates0302's extended
header handling. The channel remains OPENED on both sides, TXhead=tail0x28
and RXhead=tail0x38. Live SMP2P snapshot is state0x6/version1. Amplifier
baseline registers remain unchanged. The user confirmed responsive iOS after the query;
read-only SSH and surveys also worked.

Next is a serialized APR transaction/notification dispatcher and an isolated
ASM shared-memory map/unmap test with explicit physical-page/cache/handle
ownership. The pinned q6asm driver uses shared-memory pool3 and property0;
no buffer was allocated or mapped by this milestone. ASM session, ADM/AFE
routing, SLIMbus/codec configuration and completion-driven PCM buffers are
still required before physical playback, followed by microphone capture.

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
  PAS1 AUTH_AND_RESET subsequently passed and the DSP reported ready0x6
  with responsive iOS. Physical destination readback was not performed.
- Partition surveys found an empty APPS↔ADSP partition at SMEM+0x86000.
  Patch0288 then acquired/released remote lockS:3, initialized outbound
  SMP2P item429 and verified the complete block. An independent scan found
  one valid340-byte record at0x86030 and uncachedfree0x188, with no malformed
  records. No IPC, power vote or DSP start was requested. iOS stayed responsive.

The next audio step is shared-buffer map/unmap through a reusable serialized
APR dispatcher, building on the now-verified CORE command exchange. Q6AFE/Q6ASM/SLIMbus PCM
transport, codec routing and physical microphone capture remain to be
implemented. Output is currently discarded and input is synthetic; endpoint
counters and successful iOS API calls establish only software plumbing.

The Wi-Fi/battery audit fixes through patch0291 are committed in `236a880`.
Their host regressions and full kernel build passed. The packaged image
was accepted by temporary fastboot boot and the user confirmed responsive
iOS. Read-only battery publication passed; the Settings Wi-Fi join failed.
See [WIFI-STATUS.md](WIFI-STATUS.md), [BATTERY-STATUS.md](BATTERY-STATUS.md)
and [NOTES.md](NOTES.md) for results and artifact hashes. Audio milestones
are tied to their named tested images; none establishes a physical playback
or capture milestone.

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

##0315 muted codec route image ready (2026-11-14)

0314 channel lifetime passed with clean teardown; user confirmed iOS STILL
RESPONSIVE afterward. Phone remains0314; do not repeat one-shot controls.
0315 source c305b28c674e0ba331fd03f513add0336861fbe2 is committed and native
incremental build exited0 using recovered exact22G74 KDK outside iCloud.
Changed sweet_keys compilation and new codec-route kernel strings verified.
Existing build job and provenance: work/codec-route/incremental-provenance.json.
Image outputs/river-codec-route-c305b28.img is26820608bytes,577cmdline;
SHA35b8563890b2aefb31dbc8c7187b129f9d058bbf9a376bec00df4183d7786da6.
Kernel SHAb7fdf450234ac09e34ede71fbc13a4974bcecbab6f124a6e07d94442240b001f.
Native helper compiled/signed and embedded; kernel payload segments verified.
Temporary boot requested, awaiting user fastboot readiness; no0315 boot yet.

0315 --codec-route owns the channel test once, activates codec sink0/channel144,
then invokes existing verified muted FLL/AIF clock setup and a bounded codec
data-route callback before restoring data registers, clock state and channel.
Read-only SPI preflight rejects foreign source/rate/port/GPIO before clock writes.
Snapshots22 digital registers; routes SLIMRX1 source0x38 to both AIF1 TX slots
at0dB, GP10 native TXDAT,48k16-bit stereo slots, enables muted amp ASPRX1.
Requires RXport0 active, codec FLL lock, amp PLL lock and AMP_EN0/mute1.
Cleanup clears sources before rate restoration; persistent restore fault
retains rate protection and amp mute until reboot. No analog/mic enable.
Actual-code sanitizer tests passed route writes/locks, each ignored write,
restoration failures, preflight rejection and channel callback cleanup.
No new PCM is submitted while this codec route is active yet. Earlier PCM
prerequisite remains separate; physical playback/capture are pending.

After temporary boot, require user responsive-home confirmation and inspect
passive start/stream/slim/ngd/channel/codec-route statuses plus amp baseline.
Then run work/codec-route/run-device.py ONCE: PCM prerequisites, discovery,
power, enum, --codec-route/status, channel/status and independent Wi-Fi reader.
Do not run --channel-test first: it would consume the route's one-shot gate.
Ask post-sequence responsiveness. Next: zeroPCM while route active, bounded
quiet tone, real device backend, then microphone capture. No unmute this boot.
Wi-Fi remains secondary; no new UI join result or code change since0314.


##0315 codec route stopped with full restoration (2026-11-14)

PCM4/4 and channel6/6 ACKs passed again. Both FLL and amplifierPLL locked.
Mixer source0x700 write0x38 read0x8038 triggered exact-word rejection;
route_seen0,all codec/AIF/amp/clock/pin restoration successful, channel fully
removed/disconnected, mute retained. No active-route DSPPCM submitted.
Cirrus DS1085F2 Table4-7 bit15 is x_STSn enable status;0316 corrects only
status handling and adds explicit mixer-enabled observation and host tests.
Physical playback and capture remain pending. User confirmed post-test iOS responsive.

##0316 codec route PASS and responsive (2026-11-14)

User confirmed home and post-sequence iOS responsive. Full prerequisite
DSP/CORE/map/ASM/config/ADM/zeroPCM4/4/QMI/enum sequence passed again.
--codec-route: phase=muted_codec_route_complete error0 complete1 route_seen1,
route_restored1 bad_reg0 port_map0x100 port_before/live1 rate0 FLL0x100,
amp_irq0x2000000a,clock/channel_error0,channel_complete1. Both mixers enabled;
AIF/amp/clock/pin state restored. Channel6/6ACKs, connected/defined/active0,
uncertain0,cleanup_error0. Amp mute held. No samples submitted while codec
route was active. Current phone0316, QMIpower/NGD retained; PCM closed/unmapped,
channel removed/disconnected. Do not retry active controls. Evidence private
work/codec-route-fix/*-91e8fac.txt. Initial passive SSH timeout was followed
by complete confirmed-baseline statuses and full passing sequence.
Next0317: separate session5/permanent PCM state using shared tested engine,
run zeroPCM while verified channel/codec/AIF/clock route is active. Release
speaker lock around APR waits while retaining ADSP/codec ownership; recheck
mute/output disable/locks afterward. Physical playback/capture remain pending.
Wi-Fi0315 spinner join diagnostics preserved before reboot; current0316
monitor is historical, no new UI join result. Keep Wi-Fi secondary.


##0317 active-route PCM image ready (2026-11-14)

0316 muted codec route PASSED with full restoration and user-confirmed
post-test responsive iOS. Phone remains0316, amplifier muted, QMIpower/NGD
retained, PCM mappings/streams and data channel closed. No retries this boot.
0317 sourceb9fe8ea021eb3036f3ac9ee0b03184488acc1e9b committed before build.
Same zeroPCM engine now accepts explicit state/session; original session4
state/evidence remains intact. Separate permanent16KiB allocation/session5
runs inside active SLIMbus/codec/AIF/FLL route through --codec-pcm. Requires
prior PCM PASS and all existing gates. Physical pages checked disjoint from
map-test and first stream. Each start reapplies AFESET/GET before START.
Exact completion/address/handle checks and cleanup preserved. No tone/unmute.

ADSP+codec locks retain ownership; speaker lock released only during APR
engine and reacquired before SPI/amp checks/restoration. All amplifier I2C
transfers retain existing QUP2 serialization. AFE mute policy keeps volume
changes muted; legacy speaker tone helper now refuses while AFE owns mute.
Recheck actual mute/AMP_EN0, FLL/ampPLL, mixer sources/status and active port
after PCM. If mapping/inflight/poison/openDSP/cleanup ownership unresolved,
retain NGD channel as uncertain until reboot; codec clocks/pins/data still
restore and hardware mute remains. State/status report real AFE ownership.

Actual-code ASan/UBSan tests passed original engine regressions plus separate
session5 allocation/completion and timeout ownership preservation, actual
codec callback lock handoff/restore (modeled APR), and actual channel retained
ownership on callback uncertainty. Missing mixer-status and every ignored
route write/restore fault regressions pass. Helper compiled/signed cleanly.
Native incremental0317 exit0, exact four modified sources and new linked
string verified. KDKROOT=/private/tmp/river-kdk-22G74-qq2pjbum/expanded/KDK.pkg/Payload.
Ledger work/codec-pcm/incremental-provenance.json; prior0316 kernel preserved
work/codec-route-fix/kernel-91e8fac.verified. Existing isolated incremental
job work/build/river-3fc706c1db3d-TK5NtT; directoryname is not source identity.
Image outputs/river-codec-pcm-b9fe8ea.img:26820608bytes,577cmdline,
SHAef320c4243c5f8f867d6a7f0d1f68ba74b7a783c86a69cc5ee950bad07a94df4.
Kernel SHAc858038ec1b4ee5a19a02173de746f5fffb7f37e3d6d0bc4f250c956b41acd31.
Payload segments verified, native helperb9fe8ea and existing WiFireader.
Fastboot readiness requested; no0317 boot yet. Temporary boot only/no flash.

Next: verify productriver/hash, USBcapture work/codec-pcm/capture-boot.py,
boot exact image, ask home responsiveness, inspect passive baseline via
work/codec-pcm/passive-baseline.py, then run work/codec-pcm/run-device.py ONCE.
Do not run --codec-route or --channel-test before --codec-pcm (same one-shot
lifetime). Capture route/status and post-state/WiFi; ask post responsiveness.
After active-route zeroPCM PASS: bounded quiet tone and real completion-driven
iOS audio backend; physical microphone separate. Audible playback/capture pending.
Wi-Fi fresh0315 spinner/no-error logs preserved before reboot. Associated4/
result1 and DHCPACK but native interface rankNever/NWI absent/empty resolvers.
No new0316 join; retained monitor history is not current association proof.
No Wi-Fi override or code change in0317. Source audit still points toward
native configd/IPConfiguration service lifetime after association; owner
unresolved. Keep secondary, retain private diagnostics outside Git.


##0317 AFE start rejected, clean teardown, responsive (2026-11-14)

User confirmed home and post-sequence iOS responsive. Known standalone PCM
session4 passed4/4 again, QMI/NGD/codecLA/channel activation and codec route
passed. Active-route session5 mapped separate16KiB at0x20804000, reapplied
AFE SET/GET successfully, then AFE START0x100e5 returned matched DSPstatus1
(error71), before ASMopen/RUN/WRITE. submitted/completed0. No uncertainty:
live mapping unmapped, codec/AIF/amp/clock/pins restored, channel6/6ACKs
fully removed/disconnected, amp mute retained. route_seen1 FLL0x100,
ampIRQ0x2000000a. No active-route PCM success claimed. Current phone0317;
all active one-shots consumed, QMIpower/NGD retained. Do not retry controls.
Private evidence work/codec-pcm/*-b9fe8ea.txt, especially codec-pcm/status.
Next: audit/adjust AFE lifetime ordering relative to codec sink activation.
DSPstatus1 alone does not prove the cause. Physical playback/capture pending.


##0318 AFE-before-channel + Wi-Fi observer ready (2026-11-14)

0317 full prior stages/codec clocks and route passed; active PCM AFEstart
returned matched DSPstatus1 before any submission. Complete unmap/codec/
amp/clocks/pins/channel cleanup, no uncertainty; user confirmed responsive.
Phone remains0317, amp muted/QMIpower/NGD retained. Do not repeat one-shots.
0318 source24942bcdb51c2d588ee12b642a0e4c4455b670bd committed before build;
prepares acknowledged AFE SET/GET/START BEFORE codec channel activation in
--codec-pcm. Session5 then reuses that start, without SET/START during the
codec callback. Only exact owned afe_prepared state allows NGD transactions
while AFE active; all prior map/ASM/stream proof/mute/ownership gates remain.
No relaxed raw mode. Codec early failures close owned AFE before channel
cleanup; pre-callback channel failure also cleans AFE. Unknown ownership
retained, no retries. This is an ordering hypothesis, not a proven root cause.
Primary stock msm-dai-q6-v2.c calls AFEstart in prepare; Madera sink activation
is a separate DAPM event. Source pin work/codec-pcm-order/stock-source-pin.json.

Actual-code ASan/UBSan engine tests pass AFEprepare ordering/one start,
reuse/no duplicate SET/START, prepare rejection and owned cleanup plus all
previous completion/timeout/2-page/session tests. Actual codec callback tests
pass early prepare rejection/no writes, APR speaker-lock release, complete
restoration and uncertainty. Actual NGD/channel tests pass. Native0318 exit0,
exact three headers/new strings verified; previous kernel preserved
work/codec-pcm/kernel-b9fe8ea.verified. Build/ledger work/codec-pcm-order/
incremental-provenance.json, job work/build/river-3fc706c1db3d-TK5NtT.
KDKROOT remains /private/tmp/river-kdk-22G74-qq2pjbum/expanded/KDK.pkg/Payload.
Kernel SHAa883dcd595a0376fa372dd2e421700d827413b661c29d83f80f7ff667e99c5ff.
Image outputs/river-codec-pcm-order-24942bc.img:26839040bytes,577cmdline,
SHA8a8be9f559a03527cc96e2726749a337e3e6cf9e8b8794a3f4243685cf9735bd.
Fastboot requested; no0318 boot yet. Temporary boot only; never flash.

Wi-Fi secondary: rget read-only fingerprint proves currently installed
river-net71728B SHA807a0190cac9c3dbb3ce685418f0235af95dc025b9192ba0275fa93dbb7b44ba,
without tracked SCwrite/readback observation strings. Prior observer was
apparently overwritten by older bundled daemon on later temporary boots;
that overwrite mechanism still needs confirmation, not assumed. Initial
cat attempt exit127/0B was missing tool, not a daemon fingerprint; rget
capture succeeded exit0. Private binaries/metadata work/codec-pcm/installed-*.
Recompiled existing tracked observer source (no source/config behavior
change), same three entitlements verified against installed daemon, signed.
Only warnings deprecated daemon and unused ask_ipconfiguration. Embedded
observer SHA962ae61430553b64e3d6d3282ad43625ddc30662062447f2874f6c7fe5a6528f.
Committed packer114caea184dacb9f0b307bdfb39297cf31d06216 adds optional
--net-helper and validates every actual RiverFiles entry: unrelated content
and metadata preserved, no duplicate/missing daemon; payload/kernel verified.
No live daemon replacement/restart or new rank/route/DNS overrides this turn.

After readiness: verify productriver/hash, start work/codec-pcm-order/
capture-boot.py, temporary boot, ask responsive home, inspect passive baseline
then run work/codec-pcm-order/run-device.py ONCE. --codec-pcm owns same one-shot
as --codec-route/channel-test; never run those first. Capture full status and
post responsiveness. Also read installed daemon with rget and verify embedded
observer hash before fresh Wi-Fi join/log collection. Existing monitor history
is not current join proof. After active-route PCM PASS: bounded quiet tone,
real iOS backend, physical mic. Playback/capture remain pending.


##0318 AFE prepared, controller guard stopped, responsive (2026-11-14)

User confirmed home and post-sequence iOS responsive. Known standalone PCM
session4 passed4/4 and all QMI/enum prerequisites. New order AFE SET/GET/START
PASSED before codec channel activation: live_pcm phaseafe_prepared,error0,
session5,afe_started1. Channel test then rejected initial controller ownership
(error16) before attempted/commands/codec/PCM writes. No uncertainty. Outer
owned AFE cleanup executed; no PCM mapping allocated or sample submitted.
Old channel note predates AFE cleanup and shows1; current0318 codec status
lacks final DSP flags, so do not interpret that stored snapshot as live state.
Source cleanup sets afe_started false only after matched STOP. Next0319 adds
explicit final flags and captures exact preflight registers (0318 ownership
failure note's zero cfg/status did NOT read those registers). Do not invent
which guard failed. Current phone0318; no active retries, amp mute retained,
QMIpower/NGD retained. Private work/codec-pcm-order/*-24942bc.txt.

Installed Wi-Fi logger hash verified exact962ae614...; observer active.
User fresh join spinner; native state4/result1. All SC writes accepted0errors;
globalIPv4 readback immediate present1/end0, serviceIPv4/DNS present at both
observer points, but subsequent native config has neither serviceIPv4/DNS,
no connected/local route, configdPID35 rank3Never/NWIen0 absent/empty DNS.
This proves accepted publication subsequently disappears; owner/path still
unidentified, no override. Raw monitor/config/scalar/DNS captures complete;
kernel capture partial196545B/SSH255 timeout after data, not a failed join
or proven phone hang. Private join-*-24942bc.txt; current user responsive.

