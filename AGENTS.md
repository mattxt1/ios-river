# Working on ios-river

- **Read `NOTES.md` before starting**: It contains the project goal, hardware facts,
  current blocker, local artifact locations, and the next concrete action.
- **Agent Handoffs**: Update the active handoff in `NOTES.md` after each milestone
  and before stopping. Record commits, exact build/boot results, sysctl readbacks,
  and unfinished work. Distinguish verified observations from hypotheses.
- **Kernel Builds**:
  - Primary path: Local builds via `tools/river-local-build.sh` (see [LOCAL-BUILD.md](LOCAL-BUILD.md)).
    Requires `KDKROOT=/Users/matttyson/Documents/Codex/2026-11-05/d-2/work/local-build/kdk-full/KDK.pkg/Payload`
    and `RIVER_LOCAL_BUILD_ROOT=/Users/matttyson/Documents/Codex/2026-11-05/d-2/work/local-build`.
    Changes under `xnu/patches` must be committed to git before invoking the script.
  - Fallback path: Remote GitHub Actions via `tools/kbuild-river.sh` / `gh run`.
- **Packaging & Booting**:
  - Packaging script: `/Users/matttyson/Documents/Codex/2026-11-05/river-battery-implementation/work/package-river-audio.py`.
  - Image size must stay strictly < 38 MB; boot cmdline must stay strictly < 608 bytes.
  - Device bring-up uses temporary `fastboot boot` images only. Never flash boot,
    recovery, firmware, or partition tables.
  - The phone's eMMC `userdata` partition holds the HFS+ iOS root (`rd=emmc0s64 rootrw=1`).
- **Hardware Topology & Subsystems**:
  - **Audio Subsystem**:
    - Kernel endpoints: `/dev/hakios-audio` and `/dev/hakios-mic` (patch 0270).
    - CS35L41 smart speaker amplifier: I2C `0x40` on QUP2 (`0x078b6000`), reset on TLMM GPIO 94 (`0x0105e000`), IRQ on GPIO 63. Boost energized at 8.5V, 4500 mA peak, +17 dB analog power gain (`0x00006c04 = 0x220`).
    - CS47L35 (Madera) audio hub codec: SPI bus 7 (`0x07af7000`), BLSP2 QUP3 (ID `0x6360`, revision1 verified), reset on TLMM GPIO 68 (`0x01044000`), IRQ on GPIO 46. A muted native AIF1 clock test on GPIO9/11 achieved both codec FLL and amplifier PLL lock, then restored all state. PCM transport is still unimplemented; clock lock does not establish audible playback.
    - **Hardware Protection Interlock**: When `CS35L41_PLL_UNLOCK` is set (register `0x00010098` bit 4 = `0x10`), CS35L41 hardware safety logic enforces an automatic Hi-Z protective shutdown of the Class-D PWM H-bridge output stage. Physical sound requires a synchronous locked clock on ASP BCLK.
    - **QUP2 Concurrency**: QUP2 is shared between CS35L41 audio amplifier operations and AW8697 haptic/input worker threads. All transfers must be serialized via `lck_mtx_t ri_qup2_lock`.
  - **Battery Subsystem**:
    - Hardware JG30 fuel gauge sampling via PM8953 FG/QG (patch 0267), published to IOKit `IOPMPowerSource`.
  - **Display & Backlight**:
    - 60 Hz flip path (`sweetfb_hz=60`, `riverblit=1`), native touch (`rivertouchlatch=0`).
    - CoreBrightness TLMM PWM backlight driver (`riverbl=1`, patch 0269).
  - **Networking**:
    - WCNSS Wi-Fi (`en0`) managed by `org.river.net` launchd daemon.
    - USB CDC-ECM Ethernet (`192.168.7.2`), root SSH via Dropbear on port 22.
- **Current audio continuation**: Patch0299 authenticated stock ADSP startup passed: all four exact CX/XO ACKs, PAS1 status0/result0, slave-kernel ready state0x6 and user-confirmed responsive iOS afterward. Shared active/sleep CX384 and XO1 remain enabled until reboot by the experimental policy; do not invent baseline unvotes. The ready DSP created17 private APPS/ADSP records including item13 at SMEM+0x86300. Patch0300 live discovery passed with responsive iOS: apr_audio_svc CID6/flags0x201,40-byte controls,16KiB FIFO, host0/peer1 and empty rings; no GLINK records. Patch0301 bounded owned SMD APR CORE state query passes host/full-build/image checks; live APR validation is pending and PCM remains unimplemented. APR transport, real PCM playback and physical microphone capture remain unimplemented. Start with newest NOTES.md and AUDIO-BRINGUP.md; zero global legacy channels does not imply an empty private partition.
- Keep device dumps, serial numbers, private keys, firmware, and downloaded Apple artifacts out of Git.
