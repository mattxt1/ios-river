# iOS 16 on the Motorola Moto G7 (`river`)

Experimental bring-up of Apple's open-source XNU kernel and iOS 16.7.16
(20H392) userland on the Motorola Moto G7 XT1962-1, Snapdragon 632 / MSM8953.
It descends from [ios-sweet](https://github.com/dipa-studio/ios-sweet) and
includes work for other devices, but `river` is the target of this branch.

**This is a research port, not a production phone OS.** Device results below
are historical observations, not a guarantee about a freshly built image or the
phone's present state. Apple security services are substituted or relaxed in
parts of the port. Do not rely on the device to protect personal data or use it
for sensitive accounts. The battery and charging icon is synthetic.

The summary is reconciled against this branch's records through October 30,
2026. Source changes may be newer than the last phone test; see [NOTES.md](NOTES.md)
for dated evidence, build IDs, hashes, limitations, and later observations.
[RIVER.md](RIVER.md) is a historical bring-up narrative and stops earlier.

## Status and evidence

| Area | Recorded result | Limits |
| --- | --- | --- |
| Boot and storage | Stock Motorola bootloader loads a temporary image via `fastboot boot`; XNU mounts an HFS+ iOS root on eMMC `userdata`. | Preparing that root overwrites Android user data. A RAM-only boot does not mean the overall installation or a running writable root makes no persistent changes. |
| CPU and memory | Four Gold cores and approximately 3.38 GiB usable RAM were reported on the full-RAM configuration. | Firmware carveouts must stay excluded. Not a claim that all eight cores run. |
| Graphics | Adreno 506 acceleration was demonstrated using Mesa Freedreno, RiverGPU, and an EAGL/OpenGL ES adapter. | This does not establish complete native Metal support or 60 FPS UI. A 60 Hz display/present path is not an FPS measurement. |
| Performance | On October 28, a surface-release session recorded about 20.2 renderer flushes/s during home-page swiping and 26.1/s scrolling Settings. | Flushes are a throughput proxy, not unique frames scanned out. Measurements are workload- and build-specific. Hitches remain. |
| Display and touch | Touch and a cropped iPhone X canvas work. A later `riverbl=1` test showed the backlight off during sleep and returning on wake. | Full-canvas fit has not been demonstrated; failed scaling tests are documented in [DISPLAY-FIT-HANDOFF.md](DISPLAY-FIT-HANDOFF.md). Backlight ordering and error cases remain untested. |
| Home and switcher | On October 30 the user reported working Search, folders, app previews and sleep/wake on a candidate. A wallpaper replica remained visible in the switcher; a later wallpaper change was also reported to preserve the earlier fixes. | The replica is a workaround, not proof the underlying compositor issue is fixed. The tested binary and a separately rebuilt binary had different hashes; do not infer the latter was tested. A later blur candidate rendered pink and was rolled back. See [NOTES.md](NOTES.md). |
| Wi-Fi | WCNSS association, WPA2, DHCP, DNS, and Safari loading an HTTPS page were documented; group rekey and renewal support were implemented. | A successful browsing test does not establish behavior on every network or after extended uptime. The standard iOS Settings Wi-Fi experience is not complete. |
| USB and debugging | USB Ethernet and Dropbear root SSH were used, alongside local diagnostic tools. | Require the configured image, on-device tools and host receiver; availability after any given boot is not guaranteed. |
| Battery and charging | `SweetBattery` publishes a synthetic capacity (default 85%) and charging properties. Read-only power and charger surveys were run. | Percentage, voltage and charging icon do not establish actual battery condition. Some SPMI reads intermittently returned zero, including identity fields; gauge freshness and charge state are unverified. USB connected does not prove charging. |
| Haptics and buttons | On October 30 the user reported touch/volume feedback and confirmed separate `vibrate 50` and `vibrate 25` requests were felt and stopped. | The path responsible for touch feedback was not independently established. The Core Haptics capability probe reported `supportsHaptics=0`; normal iOS haptic integration is not established. |
| Audio, microphone, camera, torch | Hardware-source surveys and early probes exist. | No end-to-end iOS microphone, camera or flashlight implementation is established. The experimental torch pulse is disabled in production builds; do not bypass its interlock. |
| Time | A boot-argument RTC offset has been used to set the clock. | Its validity depends on the device and configuration; the recorded offset is not a universal time source or a guarantee that the clock is accurate. |

## Architecture

```text
Motorola ABL -> fastboot boot -> shim-river -> patched XNU -> iOS userland
                                                |
      touchscreen / eMMC / WCNSS / USB          +-> RiverGPU -> Adreno 506
                                                  -> Mesa Freedreno GLES
                                                  -> EAGL/GLI adapter -> UI
```

The iPhone X userland is described by the project as unmodified relative to
its original distribution, but the complete running system includes patched
XNU, compatibility services, configuration changes, injected libraries and
additional tools. Hardware and userland support are partial.

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
  recent River power, media and UI work. Entries describe their own moment in
  time and can be superseded by later entries.
- [RIVER.md](RIVER.md): earlier River bring-up history and Wi-Fi milestones.
- [RIVER-MEDIA-HANDOFF.md](RIVER-MEDIA-HANDOFF.md): hardware-source survey,
  power/torch dependencies and unverified media milestones.
- [DISPLAY-FIT-HANDOFF.md](DISPLAY-FIT-HANDOFF.md): historical failed display
  fit experiments and diagnostics.
- `shim-river/`, `xnu/patches/`, `tools/`, `phone-tools/`, `mesa/`: loader,
  ordered kernel patches, host utilities, device tools and Mesa integration.
  River patches continue through 0257; higher patch numbers do not imply a
  feature was tested on a phone.

## Next work

Validate SPMI read integrity and fuel-gauge freshness; establish actual charge
state; investigate GPU-based display fit and touch mapping; integrate Wi-Fi
with the iOS UI; characterize performance and memory; and investigate the iOS
haptic, audio and camera interfaces. Keep each hardware result tied to its
specific image, logs and test conditions.

## Credits

This work builds on [dipa-studio/ios-sweet](https://github.com/dipa-studio/ios-sweet),
[joranv1/ios-sailfish](https://github.com/joranv1/ios-sailfish),
[NWylynko/ios-flame](https://github.com/NWylynko/ios-flame), Mesa/Freedreno,
Apple's open-source Darwin components and the other sources referenced in the
engineering logs and individual patches.
