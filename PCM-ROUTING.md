# Shared PCM and physical routing continuation

## Current result through patch0329

User heard a5.12second live AVAudioPlayer/device-write PCM stream and confirmed
responsive iOS.240livebuffers through4DMA slots, zero queue underruns/overflows,
108fresh full health-cycle lease renewals, exactEOS and full cleanup passed.
Outputwindow5.15sec; muteverified5.24sec within6second hardlimit. Pre-uiomove
queue reservations reject pressure without consuming uio data; the framework
kept writing and the queue sustained all240buffers. Bitexact framework handling
of retries is not established. Readiness snapshot stayed unclaimed.

Current329one-shots consumed; protective mute retained. Automatic playback
lifecycle, explicitstop/idle/restart/recovery, longer endurance and physical
microphone remain pending. Use newest NOTES.md before controls.

## Historical prerequisite design

The following sections describe earlier bounded tests, not the current complete
speaker path or a safe instruction to repeat consumed one-shot controls.


The candidate contains prerequisite tests, not a physical audio backend.

## Same-boot test order

After temporary boot and responsive-iOS confirmation, repeat exact stock
metadata/staging/handshake/start and inspect the passive CID6 layout. Run the
one-shot CORE query, then `river-adsp-status --pcm-map`. Only a successful
`map_unmap_complete` permits `--asm-test`; `--afe-probe` is independent of ASM
success but requires verified map/unmap and an unpoisoned dispatcher. Inspect
`--pcm-status` and `--probe` afterward. The runner in work/pcm-mapping stops
on failed prerequisites and never retries controls or overwrites evidence.

Map success requires its separate nonzero handle response. A basic success
ACK is insufficient. On map/unmap uncertainty, retain the page and reboot
before another experiment. The page is also deliberately retained after
successful unmap. An ASM format NACK permits a bounded CLOSE; uncertain ASM
ownership blocks the AFE query. No sample submission, codec writes, AFE start,
ADM device or matrix route, or SLIMbus activation runs in these tests.

## Verified source facts for the next implementation

The River audio overlay is Git blob
53de11e1f7aea21a5a6e969a034d2f85dad53e2a in
LineageOS/android_kernel_motorola_sdm632. It declares:

- Madera SLIMbus elemental address `00 00 60 63 fa 01`.
- CS47L35 on SPI7, CS35L41 on I2C2, and the Madera sound card.
- Cirrus playback AFE port16384 (`SLIMBUS_0_RX`, `0x4000`).

The stock audio sources are pinned by Git blob ID:

| Source | Blob ID | Relevant behavior |
| --- | --- | --- |
| dsp/q6asm.c | de94e92a6b0b665a9a41653bb8e1bbd05e57fba3 | map pool3/property0, separate handle response, stream setup |
| dsp/q6afe.c | 9f75238c75acca09c07369e5ccdec037b6d87e07 | AFE port configuration/start and in-band GET |
| dsp/q6adm.c | 42b234c9cdc0080a99311459c8b61aa6c5735617 | device OPEN, COPP response, RX matrix mapping |
| asoc/msm8952-slimbus.c | 3f2b881be48ad81e894ec4f2346d0663488e8625 | Madera RX channels144–149 and TX128–133; codec initialization |
| asoc/msm8952-dai-links.c | 1d067ae9d8b7ff3c340ef70f995886ef55f0d7a8 | slim1 backend and AIF1/CS35L41 amplifier link |

The amplifier link specifies stereo16-bit48kHz with1.536MHz BCLK (and a
separate96kHz option). The machine driver's default SLIMbus RX channel count
is1; do not assume the amplifier's stereo wire requirement means stock
SLIMbus RX must also be stereo. Determine the codec mixer/downmix path.
Madera SLIMbus support allocates logical addresses/ports and activates the
channel group; an AFE APR command alone does not implement that controller
work. The source bridge's global `stashed_slim_dev` is Linux-specific and
cannot be copied into XNU as an operational transport.

Physical playback still requires coordinated retained DMA buffers, a bounded
notification/completion receiver, ASM stream format/RUN/WRITE, ADM device and
matrix route, AFE configuration/start, SLIMbus enumeration/channel activation,
Madera mixer/AIF routing and a verified locked/muted amplifier transition.
The existing discarded/synthetic device endpoints must not be advertised as
physical playback/capture while these are absent. Capture follows validated
playback with its own buffer and completion ownership.

##0303 verified;0304 staged route tests

The responsive0303 boot verified the16KiB map/unmap and ASM format/close
prerequisites.0304 implements explicit source-defined AFE config, inactive
ADM matrix route/teardown and muted AFE start/route/stop tests, not production
sample streaming. A rejected GET is kept separate from accepted configuration.
ADM COPP0 is valid; unresolved resources stay owned and block further tests.
Hardware-facing tests hold amplifier mute until reboot, including volume
updates, and avoid speaker/QUP locks during APR waits.

The stock SLIMbus APPS NGD has a separate QMI service0x301/version1/instance0
power interface before MMIO enable/capability exchange. Do not infer that an
AFE ACK has initialized the APPS satellite, allocated a codec logical address
or connected its shared channels. New read-only diagnostic helper reports
fixed monitor paths to investigate the independent Wi-Fi configuration gap.

##0305 readback correction

0304 AFE SET was accepted. GET returned success with52payload bytes versus
our old exact40byte expectation, so dependent routes were withheld.0305 uses
24parameter-data bytes of GET capacity and validates the self-declared24byte
record plus available bounds, preserving exact IDs/reserved/config checks.
Known field diagnostics distinguish future length and value mismatches.
Additional transport tail is ignored within the already bounded APR reply.
Synthetic padded fixtures are host tests, not recovered device payloads.

##0306 zero-only completion test

After all0305 prerequisites pass cleanly on a fresh responsive boot,
`--pcm-silence` maps its own permanent16KiB zero-filled page, opens session4,
routes the muted AFE/ADM path, RUNs, and submits four disjoint4096byte buffers
sequentially. `--stream-status` reports actual WRITE_DONE counts, bytes,
ownership, DSP errors and cleanup. Exact completion identity and status are
required; unknown ownership retains memory/mapping until reboot. The test
never unmutes or connects the iOS /dev writer to DSP. Host tests pass;
hardware result remains pending. SLIMbus/Madera connection is still required
before physical sound, even if all four buffers complete.

##0308 combined first AFE start

0306 verified routes but got DSP0x9 at its second AFE start, before PCM.
0308 follows verified cold ADM teardown directly with --pcm-silence; skip
separate --afe-route. PCM verifies mute itself and freshly SETs/GETs exact
AFE config, then performs the first hot route, RUN and WRITE sequence in one
session. First-command failure status and RX metadata survive cleanup.
Hardware completion remains pending. Do not repeat one-shot controls.

##0308 hardware completion passed

Four buffers submitted/completed,16,384bytes/4096stereo frames, status0,
all routes and mapping cleaned up, mute retained. User confirmed responsive
iOS. This proves DSP PCM consumption, not physical playback/capture.

##0319 channel reconfigure NACK; rate defect found (2026-11-14)

User confirmed0319 home responsive. Standalone PCM4/4 and knownQMI/enum
passed. AFEbeforechannel START passed. New preflight captured CFG1,
STATUS0x1a40e,IRQ_EN0,IRQ_STAT0x01000000,IE0x10000,VE0,version0x1050000;
known notification cleared, preflight2polls. Sink connect/define ACKed,
RECONFIG0x24/TID0x74 NACKstatus0x0a. Cleanup6/6ACKs fully removed/disconnected,
no uncertainty. No codec writes/PCMsubmission in active attempt. Final codec
flags explicitly AFEstarted0,DSPcleanup_error0,live_mapped0,inflight0,mute1.
Older nested livePCM snapshot afe_started1 is pre-cleanup. User post-test
responsiveness asked; reply pending. Current phone0319; no active retries.
Evidence work/codec-pcm-preflight/*-97104d3.txt.

Source re-audit found implementation error, not a proven firmware fault:
stock include/linux/slimbus/slimbus.h SLIM_CL_PER_SUPERFRAME is6144, DIV8=768.
Earlier0314 implementation/docs incorrectly used192. NGDrootfreq24576000>>3
=3072000; superfreq=4000. Madera base4000,rate multiplier12 =>coeff3/rootexp2,
so DEF_ACT third payload byte must0x20, not0. Presence remains0x83, sizebyte0x24.
Old isolated activation succeeded with an inconsistent channel rate; it did
not prove48k sample compatibility. User informed of this mistake directly.
0320 corrects exponent and adds exact pinned stock property functions/header
constants to host test; wire payload must match independently derived stock
48k values. This supersedes earlier192/exp0 assertions throughout historical
NOTES/SLIM-CONTROL entries. Hardware verification pending. Physical playback/
capture remain pending. Wi-Fi logger preserved; no fresh0319 join requested.


##0320 corrected48k channel image ready (2026-11-14)

User confirmed0319 post-sequence iOS responsive. Current phone0319; prior
standalone PCM4/4/AFEstart passed; codec sink reconfiguration NACK0x0a, then
all owned AFE/channel cleanup succeeded with explicit final flags0/errors0.
Do not retry one-shots. Physical playback/capture still pending.
0320 sourceb4c6d67fdc7aa089ea67d1b2b9cf583ab67ef283 committed before build.
Corrects DEF_ACT rate-exponent payload0x00->0x20. Stock header6144 clocks,
DIV8=768, NGDroot3072000/superfreq4000; Madera multiplier12 =>coeff3/exp2,
HARD_ISO0, presence0x83,16-bit size/coeff0x24. Old192/exp0 derivation was an
implementation error and all historical claims based on it are superseded.
User informed plainly. Earlier isolated activation ACK did not prove48k
sample compatibility. Hardware correction validation pending.

Host test now includes exact pinned GPL stock slim_calc_prrate and
slim_nextdefine_ch functions plus source header constants/enums in
 tools/tests/river-stock-slim-properties.h (source blobs in its header).
Actual outgoing wire must equal independent stock-derived properties;
48k exponent2/coeff3/presence0x83 asserted. Full actual-code NGD/channel
ACK/failure/cleanup regressions pass. Native incremental0320 exited0,
changed sweet_keys compilation/exact header/new kernel checked. Prior kernel
preserved work/codec-pcm-preflight/kernel-97104d3.verified. Ledger
work/codec-pcm-rate/incremental-provenance.json; existing isolated job
work/build/river-3fc706c1db3d-TK5NtT; exact recovered KDKROOT remains
/private/tmp/river-kdk-22G74-qq2pjbum/expanded/KDK.pkg/Payload.
Host file reads stalled on iCloud again (package script observed dataless).
Hydrated targeted scripts in place; original native build completed exit0,
no duplicate build. Do not mistake host file stalls for phone failure.
Kernel SHA5ca2143ef2e62fc6870a39516c7232d029ccddf2f2b87bfb297bee7e1d1b8f69.
Image outputs/river-codec-pcm-rate-b4c6d67.img:26890240bytes,577cmdline,
SHAfc239223856132fdec69295bd73812031ad596c1e789d44d34cbf0f932674c20.
Fastboot requested; no0320 boot yet. Temporary boot only; never flash.

Wi-Fi secondary in SAME image: commitc88a84269df28d8201bc5ce8f9b7c8fd8bd0280b
adds bounded read-only getifaddrs observations of actual kernel en0/ue0
interface flags/IPv4/mask before existing route/SC state. No configuration
writes, credential access or IPC mutations. Native helper compile -Werror
and signing pass; SHA42dc1c95e39dae609c566438142c51e2dad5fce541a2b92824418e7753dfb5f4.
Packer supports optional network-state helper and verifies all unrelated
actual RiverFiles metadata/content, kernel segments and limits. Existing
observer daemon SHA962ae614... preserved. No rank/route/DNS override.
This distinguishes actual kernel-address removal from only SC disappearance
on next spinner join. No fresh0319 join requested; last0318 evidence retained.

After readiness: verify productriver/hash, USBcapture work/codec-pcm-rate/
capture-boot.py, temporary boot, ask home responsive. Inspect passive baseline
then run work/codec-pcm-rate/run-device.py ONCE. No earlier --channel-test or
--codec-route (same lifetime). Capture active PCM status/final cleanup and
post-responsive check. Verify new network-state reader hash, then fresh Wi-Fi
join and monitor/config/scalar/DNS reads (kernel log optional bounded).
Next after corrected active PCM PASS: bounded quiet tone, real iOS backend,
physical microphone. Do not claim audible output from zeroPCM completions.


##0320 active muted PCM PASS (2026-11-14)

User confirmed0320 home responsive. Full sequence PASSED including corrected
48k exponent2 channel activation with AFE already started, codec/AIF route,
FLL/ampPLL locks and session5 PCM4/4 WRITE_DONE completions while active:
phase=muted_codec_pcm_complete,error0,complete1,route_seen/restored1,
port_live1,FLL0x100,ampIRQ0x2000000a,submitted/completed4,16384bytes,
4096stereo frames. Final AFEstarted0,DSPcleanup_error0,mapped/inflight0,
channel6/6ACKs/connected-defined-active-uncertain0, AIF/amp/clock/pins restored,
mute retained. No audible samples/output-stage enable. Current phone0320;
QMIpower/NGD retained, all PCM and data channel lifetimes closed. No retries.
Post-test responsiveness asked, reply pending. Evidence work/codec-pcm-rate/
*-b4c6d67.txt, especially codec-pcm/status/channel-status and post-slim.

Improved kernel-address Wi-Fi reader verified installed exact42dc1c95...,
52688B. Fresh Wi-Fi join requested; reply/capture pending. Historical monitor
not current association proof. Host diagnostic scripts initially stalled
before SSH due iCloud; ABSOLUTE brctl download paths hydrated scripts and
both original passive reads completed. No duplicate active sequence sent.
Use absolute hydration paths for stalled work files, not relative paths.
Next audio: bounded quiet tone through now-verified route; audit amp PCM
source/mute/power/volume and rendered completion/drain before audible test.
Continuous iOS backend and physical microphone remain pending. Do not claim
physical playback from zeroPCM completions. Exact0320 source/build/manifest
paths and KDK are in next historical entry. Keep Wi-Fi secondary.


## Live PCM queue candidate0327

`river-adsp-status --queue-arm` opts into an8192-frame CPU queue fed by actual
48k stereoS16 device writes. The HAL writer copies only; whole incoming chunks
are dropped on overflow and counted. `--queue-run` requires4096frames of
prefill and starts a dedicated kernel thread owning the existing ADSP/codec
route. Four DMA slots may be outstanding simultaneously. Token, services,
ports, opcode, physical address and map handle must all match before reuse.
`--queue-stop` and `--queue-status` remain available without taking ADSP lock.

The initial live trial is limited to12blocks and the existing750ms independent
mute deadline. It keeps the quiet attenuation/clamp and5ms edge ramps. Full
static topology is checked at unmute; dynamic monitoring rotates amp IRQ1,
IRQ3 and codec FLL lock, with one read per10ms interval rather than a93ms
survey per21.33ms PCM block. Underrun immediately mutes, drains known published
buffers through exact completions and tears down. Failed ownership checks
retain the mapping and mark transport poisoned until reboot. No fabricated
PCM fills underruns. A successful12block run requires producer replenishment
because the queue holds eight blocks. The bounded live-queue transport passed on hardware; audibility is unobserved because the user missed listening;
normal automatic continuous playback and physical microphone capture remain
pending. Never run it on a boot whose codec route one-shot was already used.

`python3 tools/test-river-live-pcm.py` compiles committed queue/pipeline and APR
decoder code with sanitizer instrumentation and injected transport failures.
The existing stream, amplifier and codec tests also apply0327 before testing.

## Runtime/backpressure candidate0328

Long mode explicitly arms240blocks with `--queue-arm-long` and launches with
`--queue-run-long`. It reserves CPU queue space before `uiomove`; a full queue
returns `EWOULDBLOCK` without consuming input. Input-copy failure releases the
reservation. The native framework's retry behavior still needs device proof.
A temporarily empty CPU queue allows22ms producer scheduling grace and receives
owned completions; permanent starvation mutes and drains exactly as before.

Only a full passing IRQ1/IRQ3/FLL health cycle within150ms may extend the750ms
watchdog lease. Expired leases cannot revive, timer callbacks never renew health,
and an independent6second cap bounds the5.12second PCM trial. The prepared muted
route waits for `--queue-go` after a listening cue;15second timeout or explicit
stop cleans up without enabling output. Short mode retains0327's original cap.
Actual cdev, queue, pipeline, lease and gate tests pass. Hardware validation is
pending. Current0327 controls are consumed; a new verified temporary boot is
required. Normal automatic playback and physical microphone remain pending.

## Automatic session lifecycle candidate0330

After muted DSP prerequisites, `--auto-enable` activates a supervised backend.
Native writes start the queue/worker automatically; no per-play route command.
Last writer close,500ms sustained silence or150ms inactivity drains accepted
buffers, renderedEOS, mute and full teardown. Later writes may restart only
following verified complete restoration and returned user-copy reservations.
The permanent mapped page is reused only after acknowledged unmap, and APR
sequence/NGD channel IDs remain monotonic. Ownership/fault/restoration failure
is sticky until reboot. `--auto-disable` drains and disables the worker.

The first hardware test limits the backend to3sessions and each session to
240blocks plus the verified6second hard cap. End-of-file may pad a partial
transport block, reported separately. HAL status advertises bounded queued
playback while enabled; mic continues to advertise synthetic capture. This
candidate has passed actual-code lifecycle/transport/lease/cleanup tests but
has not booted. Playback lifecycle hardware validation precedes physical mic.
