# Shared PCM and physical routing continuation

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
