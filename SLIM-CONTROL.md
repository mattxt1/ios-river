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
