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


##0319 temporarily booted; home check pending (2026-11-14)

Source97104d33f0043925bb66a7179d52bde2f3a98732 through0319 committed before
native build.0318 AFE-before-channel start passed but initial controller
ownership guard stopped before channel commands/codec writes/PCM. AFE
cleanup source path acknowledged STOP; user confirmed responsive. Current
phone0319 after user ready; productriver/exact image SHA verified, temporary
transfer OKAY0.612s/boot OKAY0.216s, no flash. Home responsiveness asked;
no active0319 controls run. USB-only capture session50499 under
work/codec-pcm-preflight/boot-97104d3-usb.log.

0319 captures initial version/CFG/STATUS/IRQ_EN/IRQ_STAT/IE/VE BEFORE
controller ownership rejection, plus final CFG/status/interrupts. Allows only
bounded8*5ms busy settling and documented informational W1C notices
0x11000000; never changes CFG/IRQ_EN, clears an error, or consumes unsolicited
RX to pass ownership. Unexpected RX/TX/error/unknown bits stop unconsumed.
Exact version/CFG1/IRQ_EN0/valid address remain required. New codec status
reports actual final AFEstarted/DSPcleanup_error/live_mapping/inflight outside
stored earlier livePCM snapshot. No mute/route/PCM policy change.
Actual-code ASan/UBSan channel tests pass known IE/VE notice clear+proceed,
unknown/fatal status retain, CFG change retain, stuck busy bound and previous
ACK/failure/callback ownership tests. Codec route/clock/cleanup regressions
pass. Native incremental0319 exit0, changed sweet_keys compilation, exact
2headers and new linked strings verified. Ledger work/codec-pcm-preflight/
incremental-provenance.json; prior0318 kernel preserved
work/codec-pcm-order/kernel-24942bc.verified. Existing build job
work/build/river-3fc706c1db3d-TK5NtT; exactKDKROOT remains
/private/tmp/river-kdk-22G74-qq2pjbum/expanded/KDK.pkg/Payload.
Kernel SHA81524a9d97bafc3a860916b41fa6b4c60f3a0212bd05b36d5adaaa8cd2c37183.
Image outputs/river-codec-pcm-preflight-97104d3.img:26839040bytes,577cmdline,
SHA9a3b4af91c9963cc5ee290439cdfa148b37b03396c403189058434195ebbe555.
Packer114caea validates all actual unrelated RiverFiles metadata/content,
kernel segments and image limits. Same observer/net hash962ae614... and
native helperb9fe8ea embedded; no new live network override.

Next after responsive home: inspect work/codec-pcm-preflight/passive-baseline.py,
then run work/codec-pcm-preflight/run-device.py ONCE. Do not consume channel or
codec one-shot first. Capture exact preflight registers even on failure and
final AFE cleanup flags; ask post responsiveness. If PCM passes, bounded
quiet tone then real iOS backend and microphone. Physical playback/capture pending.

Wi-Fi0318 user spinner with state4/result1 and current observer verified.
Detailed writes all accepted; globalIPv4 immediate1/end0, serviceIPv4/DNS
end1 but later native absence; rankNever/NWI absent/empty resolvers. Four
Wi-Fi reads complete; kernel partial/SSH timeout. Logs captured before new
boot. No reason to claim keyed association failure or usable Wi-Fi.
Secondary source audit of retained public bootp/ipconfig.defs: subsystem20000,
set_service at index11; existing ask_ipconfiguration is unused. Public
read-only get_summary/get_interface_list are potential next diagnostics;
private iOS16 wire compatibility not yet verified, no MIG calls made.
Do not enable the old unverified setter or force rank/routes/DNS.

