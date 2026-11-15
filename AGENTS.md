# Working on ios-river

- Read the latest active handoff in `NOTES.md` before device controls. Older
  entries are historical. Update the handoff after milestones and before stopping.
- All active work must use canonical `/Users/matttyson/Developer/Codex` paths.
  Resolve symlinks; never use Documents, Desktop or iCloud as active storage.
  `Documents/Codex` is only a compatibility symlink. See `LOCAL-STORAGE.md`.
- Keep device SSH keys in an access-restricted local directory outside iCloud.
  Never print private keys or commit credentials, raw device captures, firmware,
  KDK/Apple artifacts, build outputs or private boot images.
- Commit changes under `xnu/patches` before native kernel builds. Use
  `tools/river-local-build.sh` and `LOCAL-BUILD.md`; the current exact KDK is
  `/Users/matttyson/Developer/Codex/.dependencies/river-kdk-22G74/expanded/KDK.pkg/Payload`.
  Preserve the verified build/toolchain unless a change is necessary.
- Use `tools/river-repack-audio-diagnostics.py` for the current verified-image
  repack workflow. Images must stay strictly below 38 MB, command lines below
  608 bytes. Verify source, kernel, helper and base-image hashes.
- Device experiments use temporary `fastboot boot` only. Never flash boot,
  recovery, firmware or partition tables. Verify the device is River.
- Check fresh-home responsiveness before active controls and provide a listening
  cue immediately before an audible test. Respect one-shot ownership and never
  repeat controls with uncertain DSP/channel state or overwrite evidence.
- Through patch 0326, the user heard a chirp and a bounded replay of actual
  AVAudioPlayer/device-write PCM. Exact buffer completions, rendered EOS,
  immediate mute inside the 750 ms deadline, full teardown and responsive iOS
  are verified. Continuous app output still discards samples and mic input is
  synthetic. Do not describe snapshot replay as a continuous backend.
- Current audio work needs a device-write queue, DSP worker and matched buffer
  completion pipeline. A full hardware survey takes about 93 ms versus 21.33 ms
  per 1024-frame PCM block. Keep DSP waits off the HAL writer thread, preserve
  buffer ownership, lock order, fail-safe mute and teardown.
- Audio topology: CS47L35 on SPI7/BLSP2 QUP3 (0x07af7000), reset GPIO 68;
  CS35L41 on QUP2 I2C address 0x40, reset GPIO 94, IRQ GPIO 63. QUP2 is shared
  with haptics and requires `ri_qup2_lock`. Codec and amp clocks must lock before
  output enable. Amplifier RX slot 0 matches codec TX slots 0/1.
- Wi-Fi patch 0322 fixes the invalid zero-entry media report. Native DHCP BOUND,
  default routing/DNS, browser use, manual joins across boots and an off/on
  reconnect pass. Automatic association, renewal/endurance and IPv6 Internet
  remain pending. Never force DNS/rank to hide a native-service failure.
- USB networking failed independently on the latest boot. Wi-Fi SSH verified
  against the existing phone host key is the control fallback; do not trust a
  new network host without verifying identity. See the latest `NOTES.md`.
- Battery, display, touch and backlight status is documented in `README.md` and
  subsystem docs. Do not infer charging or other subsystem success from audio.
