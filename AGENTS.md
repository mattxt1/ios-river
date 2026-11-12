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
    - CS47L35 (Madera) audio hub codec: SPI bus 7 (`0x07af7000`), BLSP2 QUP3 (source-derived; not yet probed), reset on TLMM GPIO 68 (`0x01044000`), IRQ on GPIO 46. AIF1 signal names appear on codec GPIOs 9–11; the exact board-level connection to the amplifier is not established by the pin list alone.
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
- Keep device dumps, serial numbers, private keys, firmware, and downloaded Apple artifacts out of Git.
