# iOS 16 on the Motorola Moto G7 (`river`)

Experimental bring-up of Apple's open-source XNU kernel and iOS 16.7.16
(20H392) userland on the Motorola Moto G7 XT1962-1, Snapdragon 632 / MSM8953.
It descends from [ios-sweet](https://github.com/dipa-studio/ios-sweet) and
includes work for other devices, but `river` is the target of this branch.

**This is a research port, not a production phone OS.** Device results below
are historical observations, not a guarantee about a freshly built image or the
phone's present state. Apple security services are substituted or relaxed in
parts of the port. Do not rely on the device to protect personal data or use it
for sensitive accounts. Battery capacity remains a voltage/current estimate;
charging-policy validation is incomplete.

Updated November 14, 2026. Through0322, authenticated ADSP startup, APR,
shared PCM mapping, ASM/AFE/ADM routing, SLIMbus codec routing and active
muted PCM completion are verified. The first quiet-output attempt completed
four silent plus eight tone buffers before its independent mute cutoff;
cleanup passed. Audible result is unknown because the user wasn't listening.
0323 subsequently passed the shorter DSP/EOS sequence with full cleanup;
the user heard no chirp and iOS remained responsive.0324 corrects amplifier
receive-slot selection and has temporarily booted for the next test.
Continuous iOS playback still discards samples; microphone input is synthetic.
See [AUDIO-BRINGUP.md](AUDIO-BRINGUP.md).

On0322, a confirmed blue Wi-Fi checkmark now agrees with native DHCP BOUND,
primary en0 routing, a usable default resolver, ordinary DNS lookup, and an
unbound HTTP connectivity check through Wi-Fi. The media ioctl previously
returned zero entries, which made Apple treat the link as invalid. Safari and one off/on reconnect also pass.
A further boot/manual join also passed with Google loading normally.
Automatic association and lease-renew/endurance reliability remain pending. See [WIFI-STATUS.md](WIFI-STATUS.md)
and the latest active [NOTES.md](NOTES.md) for evidence and current ownership.
All active work uses canonical `/Users/matttyson/Developer/Codex` local storage.

## Status and evidence

| Area | Recorded evidence | Current limits (2026-11-14) |
| --- | --- | --- |
| Boot and storage | Stock Motorola bootloader loads a temporary image via `fastboot boot`; XNU mounts an HFS+ iOS root on eMMC `userdata` (`rd=emmc0s64 rootrw=1`). | Preparing that root overwrites Android user data. Temporary fastboot boot only; physical partitions are never flashed. |
| CPU and memory | Four Cortex-A53 Gold cores (`cpus=4`) and ~3.38 GiB usable RAM on the full-RAM configuration. | Firmware carveouts stay excluded. Silver cores remain parked. |
| Graphics & Display | Adreno 506 hardware acceleration via Mesa Freedreno, RiverGPU, and an EAGL/OpenGL ES adapter (`riverenv=backboardd:DYLD_INSERT_LIBRARIES=/usr/local/lib/sweetgl.dylib`). Display path runs at 60 Hz flip (`sweetfb_hz=60`, `riverblit=1`, `riverfbinv=0`). | 60 Hz display scanout confirmed. Metal is translated via OpenGL ES / Freedreno. |
| Backlight & Sleep/Wake | Hardware TLMM PWM backlight driver integrated with CoreBrightness (`0269-river-backlight-corebrightness.patch`, `riverbl=1`). Tested smooth dimming, display sleep blanking, and instant wake. | Native iOS brightness slider and display wake/sleep operational. |
| Touch & UI Interaction | Synaptics touch controller operational (`riverinput=1`, `rivertouchlatch=0`). Lockscreen unlocks cleanly at 60 fps without touch regressions. | Canvas is cropped iPhone X geometry; UI elements are fully interactive. |
| Home & SpringBoard | SpringBoard lockscreen, home screen, settings navigation, and app launch operational. App switcher functional with replica layer. | PosterBoard background load requires initial settle time on boot. |
| Battery & Power | Hardware battery monitoring via Qualcomm PM8953 FG/QG and JG30 battery curve (`0267-river-battery-jg30-power-source.patch`). Publishes observed voltage/current/status and estimated capacity to IOKit `IOPMPowerSource`. | Integrated0290/0291 preserve last-valid samples, reject invalid current and remove charger-enable writes from sampling/diagnostics. Host checks and full build pass; live SweetBattery/IOPMPowerSource/IOPS publication agrees at85%,4194–4195mV, external/notcharging/notfull with valid/nonfallback flags. Failure recovery, load/cable/charging validation remains pending; percentage is a load-sensitive estimate. See [BATTERY-STATUS.md](BATTERY-STATUS.md). |
| Wi-Fi & Networking | 0322 media ioctl correction enables a valid active link; native DHCP BOUND, primary en0/default DNS, ordinary lookup and unbound HTTP200/AppleSuccess verified with a user-confirmed blue checkmark. | Safari and one off/on reconnect pass; fresh-boot/renewal reliability pending. IPv6 Internet not established. See [WIFI-STATUS.md](WIFI-STATUS.md). |
| USB & Connectivity | USB CDC-ECM Ethernet and Dropbear root SSH (`192.168.7.2`). Driver implements bus disconnect/reconnect auto-recovery (`0244-river-usb-reconnect.patch`). | SSH and diagnostic transfer work, with intermittent USB connection/handshake timeouts observed during audio bring-up; a fresh temporary boot sometimes restored access. |
| Audio Subsystem | Active muted PCM4/4, ASM/AFE/ADM, codec/SLIMbus route, clock locks and teardown verified. Quiet output enabled and 8tone buffers completed before independent mute cutoff; full cleanup passed. | 0323 DSP/EOS passed; user heard no chirp.0324 receive-slot test pending. Continuous iOS playback still discarded, capture synthetic. See [AUDIO-BRINGUP.md](AUDIO-BRINGUP.md). |
| Speaker Amplifier HW | Cirrus CS35L41 smart amplifier probed at I2C `0x40` on QUP2 (`0271-river-cs35l41-speaker-amp.patch`). Driver configures the boost converter for 8.5V (`0x00002018 = 0x21`), 4500 mA peak, +17 dB analog power gain (`0x00006c04 = 0x220`). Reset lines released for CS35L41 (GPIO 94) and CS47L35 audio hub (GPIO 68). Tone test sysctl `debug.river_speaker_test` implemented. | Muted clock-only test verified codec FLL and amplifier PLL lock on source `fa7283f`, with complete state restoration. Physical PCM transport remains unimplemented. **Hardware Safety Interlock:** Idle raw status register `0x00010098` readback shows `irq3 = 0x20000010` (`CS35L41_PLL_UNLOCK = 0x10`). The CS35L41 Class-D output stage is held in protective shutdown by silicon logic until an external locked clock is supplied to ASP from CS47L35 AIF1 or LPASS. Concurrent QUP2 access requires serialization. |
| Haptics & Buttons | Awinic AW8697 haptic driver on QUP2 (`0x5a`). Verified on-device vibration pulses (`/usr/local/bin/vibrate 50`). Physical volume and power keys mapped via SPMI / TLMM. | Touch feedback and sysctl vibration work. Core Haptics subsystem reports synthetic profile. |
| Camera & Torch | Hardware mapping established from device tree (PMI632 flash/torch, OV12A10 rear, S5K4H7 front). Experimental torch pulse interlocked for safety. | Camera pipeline (CCI, VFE, CSI-PHY) not yet brought up in XNU. |
| Local Toolchain | Fully native offline XNU kernel compilation on macOS via `tools/river-local-build.sh` using pinned `darwin-xnu-build` and macOS 13.5 KDK. Packaging via `mkpayload` / `package-river-audio.py`. | Removes reliance on remote GitHub Actions runners for kernel builds. |

## Architecture

```text
Motorola ABL -> fastboot boot -> shim-river -> patched XNU        -> iOS 16 userland
                                                |
      +------------------+----------------------+--------------------+
      |                  |                      |                    |
   eMMC (HFS+)      RiverGPU (A506)         CoreAudio HAL       WCNSS / USB
   `userdata`       Mesa Freedreno GLES     `/dev/hakios-audio` 802.11 stack
                    EAGL/GLI -> SpringBoard `/dev/hakios-mic`   Dropbear SSH
                                                |
                                            CS35L41 Amp (QUP2 0x40, 8.5V boost)
                                            CS47L35 Hub (SPI7, GPIO68 reset)
```

The iPhone X userland is preserved relative to its original distribution,
complemented by patched XNU, compatibility HAL services, configuration plist overlays,
injected acceleration libraries, and on-device diagnostic tools.

## Storage, recovery and device rules

- The intended development boot path is `fastboot boot`; do not flash boot,
  recovery, firmware or partition tables as part of a River experiment. Verify
  device identity and the image hash before a device test.
- **Creating the iOS root on `userdata` destroys the existing contents of that
  partition.** Preserve and verify backups and obtain explicit approval before
  any destructive operation. Never assume an old backup matches this device.
- An unlocked bootloader and the correct device-specific private inputs are
  required. Keep device dumps, serial numbers, private keys, firmware and Apple
  artifacts out of Git.
- The boot payload command line has a fixed 608-byte field. Check the actual
  encoded length against the build tools' constraints before packaging; do not
  append arguments blindly.
- [RECOVERY.md](RECOVERY.md) is a historical **Redmi topaz** recovery record,
  not a Moto G7 recovery guide. Its flashing and GPT commands must not be used
  on River.

## Building and testing

This checkout is a patch series and tools, not a self-contained bootable image.
It does not include the iOS root filesystem, private firmware, or proprietary
blobs.

### 1. Local Kernel Builds (Recommended)

`tools/river-local-build.sh` builds the committed patch series locally in an
isolated directory without phone access or CI runners:

```sh
export KDKROOT=/path/to/extracted/macOS_13.5_22G74/KDK.pkg/Payload
export RIVER_LOCAL_BUILD_ROOT=/path/to/build/directory
export DEVELOPER_DIR=/Applications/Xcode-beta.app/Contents/Developer
tools/river-local-build.sh
```

Requirements: macOS with Xcode, `ccache`, `ninja`, `cmake`, `jq`, and the macOS 13.5 (22G74) KDK.
See [LOCAL-BUILD.md](LOCAL-BUILD.md) for complete details.

### 2. Packaging the Boot Image

Use `package-river-audio.py` (or `tools/mkpayload.py`) to assemble the boot image:
- Verifies boot cmdline < 608 bytes.
- Verifies output boot image < 38 MB.
- Packs the `RiverFiles` container with userland daemons (`launchd.plist`, `river-net`, `river-media-capability`, test binaries).
- Assembles the kernel, device tree (`river-wifi.adt`), GPU firmware, and WCNSS blobs.

### 3. Hardware Booting (Live Device)

Boot temporarily without flashing:
```sh
fastboot boot river-audio-boot.img
```
The device acquires IP `192.168.7.2` over USB CDC-ECM, with Dropbear SSH on port 22.
Diagnostic tools in `/usr/local/bin` (`sysctl_get`, `river-audio-test`, `river-play-sound`, `river-record-sound`, `river-speaker-test`, `vibrate`, `screencap`) allow non-intrusive runtime validation.

This checkout is a patch series and tools, not a self-contained bootable image.
It does not include the iOS root filesystem, a built kernel, private firmware,
the original boot image/DTB, the historical base payload, or the installed
on-device userland and injected libraries. Some helper scripts assume a
particular sibling-directory layout from the original work environment.

1. Read [NOTES.md](NOTES.md), [xnu/README.md](xnu/README.md), the workflow
   [`.github/workflows/xnu.yml`](.github/workflows/xnu.yml), and the headers of
   the relevant scripts before building. Record source commit, workflow run,
   artifact hashes and input provenance.
2. `tools/kbuild-river.sh` dispatches a GitHub Actions build for the pushed
   current branch, downloads artifacts and also packages a basic RAM image. It
   requires `gh`, an accessible configured GitHub repository, the relevant
   private boot image and DTB, and optionally device GPU firmware. Its default
   boot arguments are a minimal diagnostic configuration, **not** the known
   interactive UI configuration. It removes and recreates its named output
   directory. Review its header and contents before use.
3. `tools/river-mkimage.sh` packages a kernel with a River ADT, GPU firmware,
   shim, private boot image/DTB and boot arguments extracted from a historical
   sibling payload. It depends on the saved payload and other private inputs;
   an arbitrary fresh clone cannot run the example as if those files existed.
   Preserve a verified known-good configuration rather than copying old RTC
   offsets or silently changing the installed UI libraries.
4. `phone-tools/build.sh` builds available on-device helpers with the required
   Apple toolchain. Building them does not install them or prove the built
   binary matches a previously tested one. [mesa/README.md](mesa/README.md)
   describes the historical separate Mesa source and patch.
5. Test on hardware only after verifying product, source/image hashes, boot
   arguments, recovery path and sufficient power. Record observations separately
   from build success and host-model tests. Do not flash partitions for tests.

Root filesystem creation tools exist in `tools/`, but a reproducible,
independently verified end-to-end installation recipe is not established in
this README. Root images require separately obtained Apple artifacts and local
configuration. Preparing or writing one is destructive to the target partition.

## Repository guide

- [NOTES.md](NOTES.md): detailed dated engineering log, including the most
  recent River audio, power, media and UI work. Entries describe their own moment in
  time and can be superseded by later entries.
- [RIVER.md](RIVER.md): chronological River bring-up narrative, Wi-Fi milestones,
  and audio/battery subsystems.
- [RIVER-MEDIA-HANDOFF.md](RIVER-MEDIA-HANDOFF.md): historical hardware-source survey,
  power/torch dependencies and media subsystem notes.
- [DISPLAY-FIT-HANDOFF.md](DISPLAY-FIT-HANDOFF.md): historical display
  fit experiments and diagnostics.
- [LOCAL-BUILD.md](LOCAL-BUILD.md): verified local XNU build setup.
- `shim-river/`, `xnu/patches/`, `tools/`, `phone-tools/`, `mesa/`: loader,
  ordered kernel patches (currently through patch 0291), host utilities, device tools and Mesa integration.

## Next work

**Next milestone: real iOS audio input and output.** Patch 0273 adds a read-only
codec prerequisite survey and explicit synthetic/discard backend status. Patch 0274 adds an explicit, guarded codec identity-only probe. Live SPI reads verified CS47L35 ID `0x6360`, revision `0x1`, with pin restoration. The
updated tests save captures and report incomplete I/O; they do not implement
physical audio. Patch 0275 provides an explicit, isolated FLL clock test with shutdown and register restoration. See [AUDIO-BRINGUP.md](AUDIO-BRINGUP.md) for the current test
commands, source evidence and remaining codec/ADSP/SLIMbus work.

1. **CS35L41 Clock Lock & Physical Speaker Sound**:
   - Resolve `CS35L41_PLL_UNLOCK` (register `0x00010098` bit 4) so the silicon hardware safety interlock releases the Class-D PWM H-bridge output stage from Hi-Z shutdown.
   - Initialize CS47L35 over SPI7 (`0x07af7000`) to generate synchronous BCLK on AIF1 (GPIO 9), or configure LPASS speaker mux (`0x0c051004` / `csr_gp_io_mux_spkr_ctl`) with external MCLK2.
2. **QUP2 Bus Synchronization**:
   - Patch 0272 serializes individual QUP2 transactions shared by the amplifier and haptics; multi-register operations remain uncoordinated.
3. **Microphone Hardware Route**:
   - Map physical analog/digital microphones via CS47L35 ADCs to supply physical sound samples into `/dev/hakios-mic`.
4. **Camera & Imaging**:
   - Bring up Qualcomm CCI (`0x01b0c000`), VFE0 (`0x01b10000`), and CSI-PHY for the OV12A10 rear sensor and S5K4H7 front sensor.

## Credits

This work builds on [dipa-studio/ios-sweet](https://github.com/dipa-studio/ios-sweet),
[joranv1/ios-sailfish](https://github.com/joranv1/ios-sailfish),
[NWylynko/ios-flame](https://github.com/NWylynko/ios-flame), Mesa/Freedreno,
Apple's open-source Darwin components and the other sources referenced in the
engineering logs and individual patches.

### Latest device validation, 2026-11-13

Temporary kernel236a880 booted with responsive iOS. Battery state agrees
across SweetBattery, IOPMPowerSource and IOPS:85% at4194–4195mV, external
power, not charging/not full, valid and nonfallback. Six bounded snapshots
were consistent; failure recovery, load/cable transitions and charging
validation remain pending. Settings Wi-Fi joining still failed with the
nearby-channel warning and no checkmark. Patch0292 adds bounded request
completion diagnostics to investigate preflight; it makes no speculative
ABI or error-mapping change. Patch0293 corrects signed amplifier attenuation
and verifies the level before unmuting. Both pass host checks and the a6de705 full build; their device checks are pending.
Exact RPM reply validation0294 also built successfully; the combined temporary image is ready. Physical audio playback/capture remain unimplemented.


Latest bb395d6 validation: Settings checkmark and public association result1
verified. Wi-Fi-bound numeric Apple HTTP returned200 with expected content.
Ordinary DNS still fails: only a scoped Wi-Fi resolver is available and the
native general IPv4 list is empty.0296 adds missing standard link transition
events with passing host coverage; full build and packaging pass; device validation pending.
The diagnostic-only DHCP observer retains existing configuration operations;
its original on-phone binary is backed up. This is partial networking success,
not a DNS/TLS or long-term reliability milestone.

The dbf26b6 test boot exposed a full lifetime32-entry scan cache: repeated
physical scans completed but newly discovered APs were discarded.0297 adds
bounded fresh working/results caches, strongest/target-priority retention,
and one named scan for an uncached requested network. Actual-code tests and the full kernel build pass. The verified temporary image
was accepted by fastboot; responsive UI and scan/join validation are pending. The previous working Settings and
numeric HTTP results were on bb395d6; DNS remains unresolved.
