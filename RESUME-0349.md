# Resume ios-river: boot/test 0349 through the agent

Handoff prepared November 19, 2026. Read this before device work, then read
AGENTS.md, the newest NOTES.md entry, HARDWARE.md and PERFORMANCE.md.

## User's current instruction

The user has River ready in fastboot and authorized the agent to temporarily
boot and test0349, collect information about recurring pauses, then move to the
usability milestones in HACKIOS-COMPARISON-2026-11-19.md. They explicitly do not
want telephony work. They refuse a manual-boot workaround and want device work
performed normally through the agent. Preserve that preference.

They are starting a new chat because this chat has no permissions control and
its command sandbox cannot access the device. A new chat is not guaranteed to
have different permissions: verify access first. Do not repeatedly ask for cable
replugs, replace working platform-tools, or edit/bypass security policies.

## Use the existing local checkout

Repository:
`/Users/matttyson/Developer/Codex/2026-11-19/continue-my-work-on-ios-river/work/ios-river`

Branch: `codex/power-backlight-haptics`. Work through commit63b31c8 was clean
before this handoff. The handoff itself is committed afterward; inspect HEAD.
These task commits have not been pushed. Do not switch to an older checkout or
clone from GitHub and assume it contains the prepared candidate. Generated images,
builds and private evidence are outside Git and remain on this same Mac.

## Device state and access blocker

- Last actual iOS image:0345. Current state: user reports fastboot; macOS sees
  `Fastboot river S`, Motorola, serial `ZY226HM8QB`.
- **0349 has never been booted. No image was sent by this task.** No flash,
  reboot, hot deployment, audio initialization or audio enable succeeded.
- `/opt/homebrew/bin/fastboot`, version37.0.1-15733141, executes but returns an
  empty device list inside the old session. A waiting getvar was cancelled.
- SSH to192.168.7.2 failed `Operation not permitted`; `ps` was denied and
  `log show` explicitly said `Cannot run while sandboxed`.
- `CODEX_SANDBOX_NETWORK_DISABLED=1`; user config workspace-write/on-request;
  active task policy disabled sandbox approval escalation. This is execution
  environment evidence, not evidence that phone firmware or fastboot is broken.
- The prior suggestion to use Full access could not be followed because the
  permission selector was absent in that chat. Do not claim it was enabled.

## Exact candidate and rollback

Manifest:
`/Users/matttyson/Developer/Codex/2026-11-19/continue-my-work-on-ios-river/outputs/hardware/candidate-manifest.json`

0349 image:
`/Users/matttyson/Developer/Codex/2026-11-19/continue-my-work-on-ios-river/outputs/hardware/river-hardware-0349.img`

SHA256 `9e1e80630e7eb85424f34fd08fe4beca5659137dc9c094c02361b1fc3bddf4b8`.
27,168,768bytes; embedded XNU command line599bytes; riverfeedback=1,
riverbl=1, haptics enabled by the existing default. The Android-header command
line is different; the runner verifies the embedded SWEETXNU payload field.

Full0345rollback:
`/Users/matttyson/Developer/Codex/2026-11-18/river-performance/batch345/artifacts/river-ui-performance-0345.img`

SHA256 `5abd9cfbfe7aef83b9ea68dbd14e7a4eebc2b8b09f1caa66e62a42532f5e9415`.

Kernel source5f082ba2fea73f9bb35f47e50aade9efe5669768;
kernel SHA25665dfa0ecac4c6669d5ba12277e073b15ff418fc22c1e454cb313c9602b404efa.
Kernel/native arm64/signature and host ASan/UBSan checks passed. Do not rebuild
before checking the already-prepared image.0348image is superseded/do-not-boot.

0349 contains richer read-only battery/charger observations, backlight locking
and failed-transfer state fixes, and bounded AW8691 native-feedback endpoint plus
SpringBoard bridge. It **does not enable charging**, calibrate SOC/temperature,
or fully implement system-wide/Core Haptics. Physical motor is AW8691 at0x5b.

## Next actions, in order

1. Read the local instructions. Verify this new session can enumerate River
   using `fastboot devices -l`. Do not use an unbounded getvar when it cannot.
2. From the existing repository run:

   ```sh
   python3 tools/river-hardware-session.py preflight
   python3 tools/river-hardware-session.py boot
   ```

   The runner pins hashes, serial and product, then issues only `fastboot boot`.
   No new boot approval is needed: the user already requested it. Never flash.
   A successful transfer is not evidence that iOS reached home. If a boot transfer
   times out ambiguously, inspect state before retrying.
3. Get the user's fresh home/navigation/keyboard observation before controls.
   Then run `python3 tools/river-hardware-session.py collect --home-ready`.
   This uses strict existing SSH host trust, verifies the installed UI library,
   stages/readback-verifies a read-only helper, and captures hardware/battery,
   framework, audio status, kernel and two process-CPU snapshots. It does not
   initialize audio or write brightness/haptics/charger controls.
4. Inspect results, then test actual Control Center/Settings brightness, lock/wake,
   bounded haptic feedback and relevant preference/call-path coverage. See
   HARDWARE.md. Verify helper/library hashes before standalone injection. A
   helper pulse is not proof that SpringBoard, keyboard or other apps work.
5. Collect battery state across user-observed cable/load changes. No charger
   writes or enable policy are implemented. Percentage is still an estimate.
6. Capture recurring pauses using existing kernel/process evidence; optional
   `stacks --home-ready` takes point-in-time SpringBoard/backboardd/Preferences
   samples. It is not an automatic stall detector or a frame-rate benchmark.
   Preserve healthy baseline versus degraded-session measurements.

Runner host is192.168.7.2 by default, with the established192.168.1.232 fallback
only after checking the connection. Existing device identity is stored under
`/Users/matttyson/Developer/Codex/.device-ssh/`; never print/copy/commit keys or
disable host-key verification.

Private capture output:
`/Users/matttyson/Developer/Codex/2026-11-19/continue-my-work-on-ios-river/outputs/hardware/device-0349/`
Each invocation creates a unique directory. The prior empty-device attempts are
failures before transfer, not boot evidence.

## After 0349 qualification

Priorities are recurring waits/freezes, repeatable presentation timing, automatic
audio/Wi-Fi lifecycle, complete brightness/power/haptics and useful app workflows.
Do not count boots, host tests, short navigation checks or60Hz scanout as solved
usability. Late icons and app-closing animation remain open. The comparison
report documents other ports and the most relevant references.

Keep audio off under the user's existing no-audio-testing-tonight instruction
(November19). Their newer0349request did not explicitly request audio testing.
Ask only if audio work becomes necessary; continue other authorized testing.
Update NOTES.md/HARDWARE.md with observed results and commit the checkpoint.
