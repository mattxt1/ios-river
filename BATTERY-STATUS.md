# River battery and charging status

Updated 2026-11-13. Source audit baseline: applied kernel in build `river-b90f2d1a5c76-Dw0M2o`, with battery implementation introduced by [patch0267](xnu/patches/0267-river-battery-jg30-power-source.patch). The source audit used host tests; subsequent device observations are recorded below. Host reproductions establish code defects; they do not establish which defect caused each percentage jump seen on the device.

**Build/boot update, 2026-11-13:** full XNU build through0289–0291 passed
from source236a880. The verified26,697,728-byte temporary image (cmdline562)
was accepted by `fastboot boot` on productriver. The user confirmed the iOS home screen was responsive. Read-only live publication
checks passed as described below; these checks do not establish general hardware
reliability or correct charging behavior. See NOTES.md and the local
`outputs/audit-fixes-build-manifest.json` for hashes and exact artifact paths.

## Confirmed defects and integrated fixes

[Patch0290](xnu/patches/0290-river-battery-valid-samples.patch) preserves valid samples instead of inventing a replacement on failure. In baseline `SweetBattery::pollRiver`, any failed sample—including normal observer contention—publishes 85%, 4100 mV, external power and fully charged. A host reproduction changed a valid 53% sample to that fabricated state. The sampler also accepted current sentinel `0x8000` as a successful zero-current reading, changing the estimator input while reporting current valid. Patch0290 rejects incomplete voltage/current frames and retains the last complete published sample.

| Poll result | Numeric/cable/charging/full properties | Hardware/current valid | Available / stale |
| --- | --- | --- | --- |
| Failure before first complete sample | Absent; no fabricated fallback | false / false | false / false |
| Failure after a complete sample | Last complete values retained | false / false | true / true |
| Complete sample or recovery | Updated from hardware observations | true / true | true / false |

The whole-sample rule intentionally delays cable or charger-state updates when current is invalid, even if those individual status reads succeeded. It avoids combining a new charger state with incomplete voltage/current data. No ABI change or separate validity channel was added. A genuine measured zero current remains valid. An explicit debug percentage override still behaves as before.

[Patch0291](xnu/patches/0291-river-battery-readonly-sampling.patch) makes battery polling and `debug.river_battery` read-only with respect to charger control. Baseline `river_battery_sample` wrote `CHARGING_ENABLE_CMD` whenever external power was observed and charger state was 7. Both ordinary IOKit polling and the diagnostic handler call this sampler. The write ignored its result and could override a deliberate charger-disable decision. State 7 means `DISABLE_CHARGE`; it does not request charging. Patch0291 removes the command mapping and write, preserving external power / not charging / not full observations. It does not disable an already enabled charger or introduce new policy. Charging may remain disabled when firmware or another owner deliberately disabled it.

Both patches pass host regression checks and are included in the full kernel build236a880 temporarily booted on River. Live publication checks passed; failure/recovery, load, cable-transition and charging validation remain pending.

## Percentage remains an estimate

The current percentage is derived from the JG30 voltage table and instantaneous voltage/current, using a fixed 150 mΩ discharge compensation capped at 300 mV. It is not learned capacity or a complete coulomb-counting fuel-gauge implementation. The estimator is stateless and does not compensate charging voltage or measured battery temperature. Published temperature is fixed at 25°C; nominal capacity is 3000 mAh.

The source-based host test calculates 38%, 54%, and 61% at the same 3800 mV with currents of 0, -500, and -1000 mA respectively. Real terminal voltage also changes under load, so this controlled calculation demonstrates sensitivity rather than predicting an actual phone trace. Patch0290 removes fabricated fallback jumps and incomplete-current inputs; it does **not** solve every load-dependent SOC swing. No arbitrary smoothing was added to conceal estimator errors. A temperature-aware estimator and correlated hardware traces remain follow-up work.

## Charging policy evidence and limits

The matching Motorola/Qualcomm definitions name state 7 `DISABLE_CHARGE` and expose separate charger error, temperature, mode and command registers. The matching Linux driver uses an aggregated `CHG_DISABLE` vote to set the enable bit, rather than treating state 7 as a reason to enable charging. Its battery-status path also checks charging modes and termination/MMI state before claiming charging. Its MMI temperature/step policy explicitly votes to disable charging in STOP/FULL conditions.

Primary references at fixed board-kernel revision `2dba45b8e247958d5a1b15b33c6064c15af4983e`:

- [Charger state, temperature/error and enable-command definitions](https://github.com/LineageOS/android_kernel_motorola_sdm632/blob/2dba45b8e247958d5a1b15b33c6064c15af4983e/drivers/power/supply/qcom/smb5-reg.h#L35).
- [Aggregated charger-disable callback](https://github.com/LineageOS/android_kernel_motorola_sdm632/blob/2dba45b8e247958d5a1b15b33c6064c15af4983e/drivers/power/supply/qcom/smb5-lib.c#L1255).
- [Battery-status interpretation](https://github.com/LineageOS/android_kernel_motorola_sdm632/blob/2dba45b8e247958d5a1b15b33c6064c15af4983e/drivers/power/supply/qcom/smb5-lib.c#L1494).
- [MMI charging-step and temperature policy](https://github.com/LineageOS/android_kernel_motorola_sdm632/blob/2dba45b8e247958d5a1b15b33c6064c15af4983e/drivers/power/supply/qcom/smb5-lib.c#L5580).
- [Reference SOC direction and catch-up handling](https://github.com/LineageOS/android_kernel_motorola_sdm632/blob/2dba45b8e247958d5a1b15b33c6064c15af4983e/drivers/power/supply/qcom/qg-soc.c#L126).

Future explicit charger policy must honor JEITA/thermal and MMI voters, charger errors, mode and current/voltage limits. River's observation-only reporting is not an implementation of that full policy. This audit does not establish that autonomous PMIC protections are disabled, nor does it prove charging is correct across temperature, load or cable transitions. The existing charge-register diagnostic can provide observations; battery polling should not change policy as a side effect of reading.

## Source references and validation

Baseline applied source locations:

- `bsd/dev/sweet_keys.c:3502`: JG30 LUT and `jg30_estimate_soc`.
- `bsd/dev/sweet_keys.c:3558`: `river_battery_sample`; observer contention, VI decode, charger interpretation and removed enable write.
- `bsd/dev/sweet_keys.c:3608`: `debug.river_battery` handler, sharing the sampler.
- `bsd/dev/sweet_keys.c:3653`: separate charge-register observation sampler.
- `iokit/Kernel/arm/SweetBattery.cpp:109`: published capacity, temperature and validity properties.
- `iokit/Kernel/arm/SweetBattery.cpp:316`: 10-second River timer.
- `iokit/Kernel/arm/SweetBattery.cpp:336`: `pollRiver` and removed fabricated fallback.

Run from the repository root:

```sh
python3 tools/test-river-battery.py
```

The [test runner](tools/test-river-battery.py) extracts the actual baseline functions/LUT from patch0267, applies the actual patch hunks, and compiles baseline,0290-only and0290+0291 host variants. The [host harness](tools/tests/river-battery-host.cpp) mocks SPMI/IOKit boundaries and exercises the actual sampler, polling method and diagnostic handler. It reproduces the old defects and verifies first-sample unavailability, retained values during contention and each read failure, VI sentinel rejection, plausible voltage bounds, valid zero current, recovery flags and zero charger-control writes for external power plus `DISABLE_CHARGE`. Both baseline paths demonstrably write before0291; both issue zero writes after it. The patched disabled/not-charging state is preserved.

Result: PASS. Sequential `git apply` checks against copies of the b90 applied source also passed and produced files identical to the tested snapshots. The source audit did not perform device tests. The primary agent subsequently completed the full XNU build, temporary boot and live publication checks described here. Next checks are on-device failure/recovery and cable-state observations, plus a timestamped voltage/current/status/percentage trace across controlled load changes before changing the SOC estimator.

## Live publication check, 2026-11-13

The primary agent ran the existing read-only `battcheck` against temporary kernel236a880 after the user confirmed the iOS home screen was responsive. `SweetBattery` and `IOPMPowerSource` published the same observed state: 85%, 4195 mV, 0 mA, available=true, stale=false, hardware/current valid=true and fallback=false. External power was connected, charging=false and fully charged=false. `IOPSCopyPowerSourcesInfo` exposed one internal battery at 85% on AC Power, not charging and not charged. This verifies publication through the iOS power-source APIs for the observed state.

The 85% reading is consistent with the current JG30 lookup estimate at 4194–4195 mV and measured zero current; it is not evidence of the removed 85% failure fallback. The validity/fallback flags and actual voltage distinguish this successful sample from the previous fabricated 4100 mV/full fallback. This check does not independently calibrate current or actual remaining charge.

Six bounded snapshots recorded by the host from **2026-11-13 07:05:51.448703 UTC through 07:06:06.145560 UTC** all exited successfully. Percentage remained 85%, voltage ranged from 4194 to 4195 mV, current remained 0 mA, and all availability/validity and external/not-charging/not-full flags stayed unchanged. The roughly 15-second window spans only a small number of 10-second driver polling periods; six property reads are not six independently established PMIC samples. This is a publication consistency observation, not a load or endurance test.

The phone clock was incorrect and printed June20 dates. Use the host timestamps above for this validation. Local evidence remains in workspace `work/audit-device-validation/battery-published.txt` and `battery-series.json`; these are diagnostic artifacts outside the repository and may contain device metadata.

No transient read error, current sentinel, observer contention, first-sample absence, stale-to-valid recovery, cable transition or controlled load change was observed or induced during this check. Those paths have host coverage only. No charger-control-write trace or charge-current/temperature policy validation was collected on the device. External power plus not-charging is a faithfully published observation; its cause was not established by these snapshots. No additional battery patch is justified by this limited trace alone.
