# SLIMbus control continuation

0308 verified four zero-PCM completions with responsive iOS and full cleanup.
0309 is a hardware-unvalidated control prerequisite, not physical playback.

After a fresh temporary boot and responsive-home confirmation, run the
verified stock startup/CORE/map/ASM/AFE/cold-ADM/combined-PCM sequence.
Only exact successful PCM completions and clean resources permit
`river-adsp-status --slim-discover`. It owns only the unique CID2 IPCRTR
CLOSED/OPENING empty layout, advertises IPC-router V1 and learns the peer.
Only exact SLIMbus service0x301/version1/instance0 discovery permits
`--slim-power`, which requests hardware instance0, ADSP master mode2,
then synchronous power ACTIVE2. `--slim-status` reports protocol metadata.
The amplifier remains muted. No codec routing, bus MMIO, unmute or physical
capture is implemented. Acknowledged power and owned channel are retained
until reboot; uncertainty is not retried. Discovery failures attempt only
owned-channel CLOSED and leave malformed receive data unconsumed.

The QMI parser requires matching response node/port, message, transaction,
complete declared TLV bounds and exactly one common result record. Optional
TLVs are bounded. V1 router data padding is separate from declared payload
length. Flow-control ACKs use the source-defined RESUME_TX endpoint identity.
Host sanitizer tests exercise actual added code and modeled SMD/DSP replies;
they do not prove firmware service behavior or powered hardware registers.

Primary stock source references (LineageOS/android_kernel_motorola_sdm632,
lineage-18.1), with Git blob IDs calculated from retained exact source bytes:

| File | Git blob ID |
| --- | --- |
| [drivers/soc/qcom/ipc_router_core.c](https://github.com/LineageOS/android_kernel_motorola_sdm632/blob/lineage-18.1/drivers/soc/qcom/ipc_router_core.c) | 0e5571d4a87a009d9187d10dd9584407aa600ff2 |
| [include/linux/ipc_router_xprt.h](https://github.com/LineageOS/android_kernel_motorola_sdm632/blob/lineage-18.1/include/linux/ipc_router_xprt.h) | bba7179144b2229f354d2ab189b44719d741d4bd |
| [include/linux/ipc_router.h](https://github.com/LineageOS/android_kernel_motorola_sdm632/blob/lineage-18.1/include/linux/ipc_router.h) | c18290f9b284a8df34392f12f32f619d67ce3e4f |
| [drivers/soc/qcom/ipc_router_smd_xprt.c](https://github.com/LineageOS/android_kernel_motorola_sdm632/blob/lineage-18.1/drivers/soc/qcom/ipc_router_smd_xprt.c) | c7224515fc4dadffb1b6e85b052e00c77c31141e |
| [drivers/soc/qcom/qmi_interface.c](https://github.com/LineageOS/android_kernel_motorola_sdm632/blob/lineage-18.1/drivers/soc/qcom/qmi_interface.c) | 0b952a48a33e77c41f6224207238e074525bfde0 |
| [include/linux/qmi_encdec.h](https://github.com/LineageOS/android_kernel_motorola_sdm632/blob/lineage-18.1/include/linux/qmi_encdec.h) | 66c3d84485eda6eddd1bbfb4a2de91fcc391f9e1 |
| [drivers/slimbus/slim-msm.c](https://github.com/LineageOS/android_kernel_motorola_sdm632/blob/lineage-18.1/drivers/slimbus/slim-msm.c) | 1cc5d8476630f7a90f86708c0e01ddb961128bc3 |

##0309 observation and0310 correction

0309 PCM passed with responsive iOS. CID2 opened and peer consumed HELLO,
but local validation rejected a V1 HELLO from node5 to destination0xffffffff.
CID2 CLOSED/CLOSING acknowledged; no QMI sent and no MMIO accessed.0310
accepts broadcast router control destinations (stock dummy destination),
while DATA/QMI stays exact unicast. Control words, ports and flags are now
retained before rejection. Tests/build pass; hardware result pending.

##0311/0312 firmware endpoint and powered snapshot

The0310 capture has valid HELLO checksum and source/destination control ports
0xfffffffe, matching [Linux QRTR](https://github.com/torvalds/linux/blob/master/include/uapi/linux/qrtr.h).
0311 uses this reserved endpoint for HELLO/control receive and RESUME_TX;
QMI DATA still requires exact unicast endpoints.0312 adds --slim-ngd after
successful select/power only, matching stock ngd_slim_power_up: version then
cell-index1 NGD registers. It reads CFG/STATUS/INT_EN/INT_STAT/IE/VE, without
controller writes, RX-message consumption, reset, IRQ clear or BAM setup.
Snapshot map is0x0c140000/0x4000; version upper16 chooses stock0x800/0x1000.
Host tests and build pass; firmware QMI and actual MMIO results are pending.

##0312 hardware verified

Peer5/service0x301/encoded instance1/node5/port3 discovered. QMI SELECT and
ACTIVE result0/error0. NGD version0x01050000, offset0x1000, CFG0, STATUS0x40c,
INT_EN0, INT_STAT0, IE0x10000, VE0. No MMIO writes. PCM and all teardown passed;
user confirmed responsive iOS. Power vote retained until reboot.

##0313 polled controller/address candidate

After successful0312 prerequisites, --ngd-enumerate claims only exact disabled
NGD, enables CFG1 with IRQ_EN0, and polls register messages. It requires master
capability and logical-address assignment before source-defined satellite
report, then queries exact Madera elemental address with unique TID0x71.
No BAM/DMA, message queues, data channel, codec or AIF routing. Successful
controller is retained; on error only own CFG transition is disabled.
Malformed receive messages are not acknowledged. Host tests/build pass;
actual capability exchange and codec address are pending hardware validation.

##0313 hardware verified

Capability/report/query passed. Exact codec elemental address returned
logical0x8a; CFG1/status0x1a40e, INT_STAT0, IE0x10000, VE0. Two RX and two
TX messages, no DMA/IRQ enable/data channels. User confirmed responsive iOS.
Controller/power retained until reboot. Physical routing still pending.

##0314 muted channel lifetime candidate

After verified enumeration, --channel-test connects dynamic codec LA sink0
to channel144, defines/activates mono48k16-bit, commits, removes, commits and
disconnects. Parameters come from pinned Madera and SLIMbus scheduler code:
rootfreq3072000, coefficient3/rootexp0/HARD_ISO0, sample byte0x24, exact
presence0x83. Fresh TIDs match generic ACK success bit0x20. Unknown completion
halts cleanup and retains ownership until reboot; known NACK permits cleanup
of accepted resources. No AFE start, codec/AIF routing, DMA or unmute.
Host tests/build pass; manager/codec channel behavior awaits hardware test.

##0314 hardware verified

Six commands/six matched ACKs, TIDs0x72–0x77, final ACK0x20. Codec0x8a/sink0/
channel144,48k16-bit. All connected/defined/active flags0 after cleanup,
uncertain0/cleanup_error0. NGD CFG1/status0x5c40e/INT_STAT0. User confirmed
responsive iOS. No physical codec AIF/sample route yet.

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


##0316 mixer-status correction ready (2026-11-14)

Source91e8fac1403fe251e8b5eda0623efbb31f974d62 committed before native build.
Patch0316 handles digital mixer source bit15 x_STSn as live enable status,
compares configuration excluding that bit, and separately requires both
AIF1TX mixer inputs enabled during100ms muted observation. GP10_LVL bit15
also excluded from configuration checks. Same route/mute/PCM/channel policy.
Actual-code sanitizer model now sets x_STSn when source is selected; full
success, each ignored write, restore failures, preflight gates and missing
mixer enable timeout pass. No new physical playback/capture implementation.
Native incremental0316 exit0, changed sweet_keys compilation, exact modified
headers and new kernel checked; provenance work/codec-route-fix/incremental-provenance.json.
KDKROOT remains /private/tmp/river-kdk-22G74-qq2pjbum/expanded/KDK.pkg/Payload.
Image outputs/river-codec-route-fix-91e8fac.img:26820608bytes,577cmdline,
SHA56890ade85bcdf267747f95154b395534892e823f2366a9592d8dda5e028578f.
Kernel SHA6641561cb1480523bd7f67eeae1719924dae72cca9e2c045ae4ddec194f9e8a1.
Existing helperc305b28 unchanged; payload segments verified. Prior kernel
preserved work/codec-route/kernel-c305b28.verified. Fastboot requested after
Wi-Fi attempt; no0316 boot yet.0315 post-test responsiveness reply pending.
Phone0315 retains muted amp/QMIpower/NGD; all PCM and data channels cleaned.
Do not retry active controls on0315.

On readiness: verify productriver/hash, captureUSB using work/codec-route-fix/
capture-boot.py, temporary fastboot boot only. Ask responsive home, then inspect
passive baseline and run work/codec-route-fix/run-device.py ONCE. Never run
--channel-test before --codec-route. Capture post-restoration and Wi-Fi states.
Next after route PASS: integrate zeroPCM while channel/codec route active,
then bounded quiet tone, real device backend and physical microphone capture.


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

