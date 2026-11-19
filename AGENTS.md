# Working on ios-river

- Latest task: battery/backlight/native-feedback candidate0348. Phone remains
  last-known0345; SSH denied by session permissions, no device controls or
  audio setup/test succeeded. Read the newest NOTES.md and HARDWARE.md.
  Full Core Haptics, charging enable/policy and calibrated SOC remain open.
  Work on branch codex/power-backlight-haptics in the Nov19 task checkout.

- Current image 0345: user confirms Spotlight, settled Search pill and folders
  look good. No more audio testing tonight (November 19); do not initialize or
  enable audio on this boot tonight. Latest clear/quicker playback/navigation
  confirmation is 0344, whose enabled service ended at reboot. Record and push
  the checkpoint before further changes. Late home-screen icons remain open;
  no animation/FPS improvement claim. Read the newest NOTES.md handoff.

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
- Patch0327 has a dedicated DSP worker,8block device-write queue and4ownedDMA
  slots. A bounded12block live trial passed ongoing refill, exact completions,
  EOS,377ms mute and full cleanup; user missed listening; audibility is unobserved.
  Dynamic checks peaked12.5ms versus93ms full surveys. ALL0327route one-shots
  are consumed; do not repeat them. Runtime/lifecycle, health-based watchdog
  renewal, lossless producer backpressure and idle/recovery remain next.
  Keep DSP waits off the HAL writer, preserve ownership, lock order and mute.
- Audio topology: CS47L35 on SPI7/BLSP2 QUP3 (0x07af7000), reset GPIO 68;
  CS35L41 on QUP2 I2C address 0x40, reset GPIO 94, IRQ GPIO 63. QUP2 is shared
  with haptics and requires `ri_qup2_lock`. Codec and amp clocks must lock before
  output enable. Amplifier RX slot 0 matches codec TX slots 0/1.
- Wi-Fi patch 0322 fixes the invalid zero-entry media report. Native DHCP BOUND,
  default routing/DNS, browser use, manual joins across boots and an off/on
  reconnect pass. Automatic association, renewal/endurance and IPv6 Internet
  remain pending. Never force DNS/rank to hide a native-service failure.
- USB SSH recovered on0327 after the0326 stall; large-copy timeouts occurred
  while both transports remained usable. The prior USB root cause is unknown.
  Trusted Wi-Fi SSH is the fallback; retain existing host-key verification.
- Patch0329 audible5.12second live PCM PASSED:240buffers/4slots, zero queue
  underruns/overflows,108health renewals, EOS/full cleanup and responsive iOS.
  Muteverified5.24sec within6sec hardcap. ALL329one-shots are consumed; no retry.
  Current amp is muted, maps/routes closed. Automatic lifecycle/restart/recovery,
  endurance and physicalmic remain pending. See newest NOTES.md.
- Patch0330 automatic native-write start/silence drain/guarded restart PASSED
  two sessions/one restart,65536frames, zero queue underruns/overflows and full
  cleanup. User missed listening; audibility unobserved. Backend was disabled
  and worker exited. ALL330one-shots consumed; amp muted/maps/routes closed.
  Mic implementation is next; capture remains synthetic. Follow newest NOTES.
- Future host audio runners must redirect app SSH stdin to DEVNULL so the
  child cannot consume the parent's cued GO. A single verified directGO was
  used on329 after confirming go0/waiting1; never resend without exact state.
- Battery, display, touch and backlight status is documented in `README.md` and
  subsystem docs. Do not infer charging or other subsystem success from audio.

- Patch0334 bounded microphone TRANSPORT passed48exactREADcompletions/98304mono
  S16samples48k/fullDSP+codec+clock+pincleanup and physicalstash cdev export.
  User missed speech cue; acoustic response UNVERIFIED (peak14), iOS responsive.
  Current334 one-shot consumed, stash exhausted; no active retry on this boot.
  Repeat speech validation on fresh SAMEverified0334 image; start counting
  BEFORE user confirms so capture catches speech. Default remains synthetic,
  explicit bounded physical snapshot is not continuous microphone input.

- Cued0334 speech capture PASSED: user recognized clear crisp voice/counting
  in 2.048sec physical snapshot;48exactREADcompletions, fullcleanup and iOS
  responsive. Evidence work/microphone-speech; originalpeak544, review48x/no
  clipping. Current334consumed/stashexhausted/routesclosed/ampmuted. Noactive
  retry. Continuousphysicalmic/gaincalibration remainpending; do not conflate
  this explicit snapshot with continuous input. Useuniqueper-run exportWAVname.

- Latest335 combined transport passed18sec live mic+3sec restart/fullcleanup,
  and user heard speakers before/after, iOS responsive. Acoustic review:0dB
  andhot+12dB continuoussectionsCLEAR; +12dB restartclipcrackly/chopped. DoNOT
  claimrestartquality/gaincalibrationcomplete. Both335backendsoff/routesclosed/
  ampmuted; noactive retries.0336unity-firstADCstartup/timestampcandidatebuilt,
  hardwarevalidationpending. ReadnewestNOTES forexactimage/evidence/procedure.
- StoragecleanupNov18 preservedsource/currentnativeinputs/keys/allWAVs/images;
  oldduplicateKDKPayloadpathnowlinkstocanonical.dependencies KDK. Reports and
  SHAaudits:/Users/matttyson/Developer/Codex/2026-11-18/storage-cleanup. Historical
  generatedbuildproducts/oldcompiler/updatercachesremoved; don'tassumetheir
  generatedoutputsremain. Currentnativebuildtreeisintactand0336buildpassed.

-0337 normal iOS app candidate combines one half-duplex route owner, queued
 mic input and normal volume/session lifecycle. Native/hardware validation
 pending; do not claim everyday audio complete. Use stock Voice Memos with
 normal consent. Initialize muted prerequisites once, then --ios-enable once;
 no per-app backend swaps. --ios-disable drains both. Current phone336/off;
 consult newest NOTES before device controls.
