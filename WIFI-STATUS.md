# River Wi-Fi implementation audit and handoff

Updated 2026-11-13. The `4d238b8` temporary boot with0292 diagnostics proves successful AP authentication, association, WPA key installation and protected DHCP OFFER exchange, while Settings still shows a spinner/no checkmark and the same channel warning. Patch0295 corrects a demonstrated Apple80211 association-result mismatch: successful completion was reported as0 (pending/unavailable) instead of1 (success). Host regressions pass;0295 still requires build/device validation. The audit agent did not access the phone; observations and private logs were supplied by the main agent.

**Build/boot update, 2026-11-13:** full XNU build through0289–0291 passed
from source236a880. The verified26,697,728-byte temporary image (cmdline562)
was accepted by `fastboot boot` on productriver. The user confirmed a responsive iOS home screen. A Settings join failed with
no checkmark and a channel-congestion alert. Association reliability remains
unverified; this image did not resolve the observed connection failure. See NOTES.md and the local
`outputs/audit-fixes-build-manifest.json` for hashes and exact artifact paths.

## Source-proven failures

| Finding in the baseline | Effect | Patch0289 change |
| --- | --- | --- |
| `bsd/dev/sweet_keys.c:884` handles received deauthentication/disassociation by calling `river_scan_xact()`. That transaction polls DXE before the outer RX loop has reclaimed its descriptor. | The same received descriptor can be processed recursively during cleanup. | RX closes the data port and schedules cleanup. The owning worker performs each cleanup once after RX returns; generation ownership prevents old cleanup deleting a newer BSS. |
| RX clears WPA stage to zero, while reconnect requires stage three. No baseline assignment enters stage three. The old reconnect branch also omits data-port/UI success publication. | Spontaneous AP loss cannot reliably recover, even if association eventually succeeds. | Preserve the current target on spontaneous loss; make up to five retries with bounded backoff and publish confirmed keyed success through the shared path. Missing cached target, failed association, and failed WPA do not strand subsequent attempts. |
| Apple80211 disassociate and radio-off update UI state without cancelling the worker or its completion. `pending_join` is replaced while the old blocking join still runs. | An old failure can overwrite a new join, or an old success can reopen after disconnect/off. | Give requests internal generation identities. Reject stale completion, loss and activation; explicitly stop retries for user disconnect/off; drop queued data. Serialize final data-port activation with generation validation. |
| Completion selects the first matching SSID; target lookup silently falls back to another BSSID. | Displayed AP can differ from the AP actually selected, including a request specifying a BSSID. | Preserve the selected BSSID in completion and respect an explicit BSSID in lookup. |
| Interface media status always reports active. | Networking and UI can disagree with the actual data-port state. | Report active only while the current generation is joined and the data port is open. |

Baseline line references above identify pre0289 source, not positions in the patch file. Patch0289 modifies `bsd/dev/sweet_keys.c`, `bsd/dev/river_apple80211.c` and the internal declarations in `bsd/sys/river_apple80211.h`. Its generation field belongs to an internal structure, not the Apple ioctl ABI.

## Private boot-profile compatibility

The initial `riverwifi`/private PMK path remains supported. It adopts generation zero and uses the same confirmed-success publication and spontaneous-loss retry path as a UI join. The first explicit UI join, disconnect or radio-off invalidates generation zero, so startup cannot override a newer user request or report a stale initial failure. Power-off before the boot-profile attempt prevents association. Saved credentials are cleared when abandoning a connection. A pre-existing DMA-stall flag remains set: a new join must not pretend that a stuck DMA ring recovered.

## Device validation and the reported frequency/congestion warning

The user's photo confirms “Unable to join network” with an explanation that the network uses Wi-Fi channels shared by nearby networks and suggests restarting the router. That text is an observed UI result, not proof of actual RF congestion.

The `236a880` captures contained no association request20, prompting0292 instrumentation. **That observation is superseded by the new `4d238b8` trace**, which includes SET20 followed by successful authentication/association responses, M1/M3 processing, PTK/GTK installation, M4 completion and a matched protected DHCP OFFER. The diagnostic probe intentionally requests no lease, so an OFFER proves encrypted bidirectional packet exchange, not a configured IP address. The UDP logger began late on the newer boot; earlier initialization records are absent.

After keyed success, iOS successfully reads the selected BSSID, SSID, channel, RSSI and authentication data. GET21 then returns scalar0 with no payload. This is a concrete mismatch with the scalar Apple80211 association-result contract:0 means no completed result,1 means success,2 means generic failure. Patch0295 translates the worker's internal status into that public enum in both generation-aware completion and the legacy completion wrapper. Init/new join remain0; boot-profile and automatic-retry completion already use the shared generation-aware path. Link loss/disconnect continue to update link state and retain the last completed join result; a new join resets it. Generation/cancellation and scan behavior are unchanged.

The exact iOS source corroboration is the prior `ios-flame` patch `0123-flame-en0-apple80211-wifid-over-wland.patch`: GET21 at line1085 documents zero as not completed, and JOIN_DONE at line1357 maps internal success to1 and generic failure to2. Its CARD_CAPABILITIES comment also explicitly documents the iOS raw-byte ABI, reinforcing why macOS structure rewrites were inappropriate. The Apple-origin [association-result enum maintained by itlwm](https://github.com/OpenIntelWireless/itlwm/blob/master/include/Airport/apple80211_var.h) independently agrees. The live scalar GET21 trace matches the prior iOS implementation's transport.

Patch0292 replaces the unbounded SET-entry log with completion metadata for GET and SET: sequence number, operation, numeric type, input/output lengths, returned scalar and errno. The trace reads no user buffers, credentials, network names, BSSIDs or pointers. Atomic saturating counters cap successful calls at two and failures at four independently for each direction/type/result class, with a shared bounded bucket for out-of-range types. Repeated scans and unsupported vendor requests cannot exhaust another type's budget. It takes no engine/net locks and introduces no extra user-memory copy.

The user-visible congestion/channel explanation still has no verified exact error-to-string mapping. The earlier generic values1/16 were copied into the public result field;1 is actually success in that enum. Patch0295 reports2 for generic failure rather than claiming a specific RF/cipher diagnosis, and reports1 only after confirmed keyed success. This fixes result semantics; it does not suppress a real association failure or claim to solve every cause of the warning.

Next device validation should confirm GET21 returns1 after the WPA gate, check whether Settings gains its checkmark, and inspect IPv4/DHCP configuration. If UI failure persists despite the corrected result, inspect link-event/network configuration behavior using actual iOS evidence. Optional unsupported getters in the current trace are not sufficient reason to invent successful answers.

## Validation performed

Patches0292 and0295 apply cleanly after0289 against the integrated engine. The actual-code state regressions compile and pass with both included:

```sh
python3 tools/test-river-wifi-state.py --kernel-source /absolute/path/to/pre0289/xnu --extra-patch xnu/patches/0292-river-wifi-ioctl-preflight-trace.patch --extra-patch xnu/patches/0295-river-wifi-association-result-abi.patch
```

The `--extra-patch` option applies additional patches only to the temporary test copy. The0295 public ioctl tests explicitly check GET21 pending0, successful keyed completion1, failed attempt2, a subsequent join resetting0, retry/boot-profile success and legacy-wrapper translation. Existing stale-completion tests now expect the correct public values. The pre0295 baseline regression mode also passes. These tests preserve behavioral regression coverage; they do not claim to reproduce the private iOS consumer's preflight decisions. A separate host stress check of the extracted trace helper passed with eight concurrent threads and 800,000 calls: the shared type emitted exactly two success and four failure records, SET retained an independent budget, a later association type retained its budget, and negative/out-of-range types stayed bounded.


Run from the repository, with a kernel source tree containing patches through0288 and excluding0289:

```sh
python3 tools/test-river-wifi-state.py --kernel-source /absolute/path/to/applied/xnu
```

The runner applies0289 to copies of the three source files in a temporary directory and compiles the actual modified Apple80211 engine, RX handler and worker transition helpers against deterministic hardware stubs. It does not alter the kernel source or repository index. The compiled regressions passed with `-Wall -Wextra -Werror` (unused static functions/variables allowed for extracted source).

Coverage includes stale completion and stale link-loss rejection; exact BSSID selection; success clearing a previous failure; real RX handler deferring all SMD transactions; one cleanup per owner; old cleanup skipping a newer BSS; automatic reconnect reopening data and UI state; disappearance/reappearance of a target; association and WPA failure followed by another retry; retry exhaustion; user disconnect and radio-off cancelling retries; a newer join arriving during the old WPA operation; cancellation injected between completion and data-port activation; and boot-profile success, reconnect, cancellation and supersession. Source assertions verify RX cannot call `scan_xact()`/DXE polling and that the outer worker processes connection service before queued network output.

## Still unresolved

- Build and temporarily boot patch0295; confirm successful GET21 result1, Settings checkmark and a real configured DHCP lease. Patch0292 device validation already proved WPA/protected-packet success. Reconnect under AP loss, wrong credentials, repeated Settings joins, radio toggles and UI error reporting remain unverified.
- The fixed-size 32-entry BSS cache has no expiry/replacement policy. Target lookup/recovery uses cached records; fresh scan scheduling and channel changes are not solved by this patch.
- The UI target path synthesizes a WPA2 RSN choice and marks rates/security accepted. It needs capability validation comparable to the strict beacon-selection path. Current support remains narrow WPA2 PSK/CCMP; this is not WPA3/enterprise/roaming support.
- A stuck TX DMA ring still needs explicit, verified recovery. Clearing its stalled flag during a new join is insufficient.
- Suspend/resume, firmware-reset recovery, long-running packet loss and broader network compatibility need hardware coverage.
- No claim is made that this audit found every cause of the user's intermittent connection failures. Its fixes address demonstrated state-machine and RX reentrancy defects.

## ABI cautions for the next investigation

Comparison with the Apple-origin headers maintained by the [itlwm driver](https://github.com/OpenIntelWireless/itlwm/blob/master/include/Airport/apple80211_ioctl.h) identifies suspicious replies: current GET14 returns MAC bytes although the header names14 PHY_MODE; GET23 returns scalar20 although the header describes STATUS_DEV_NAME; several other getters use only `req_val` where that implementation uses versioned structures. This is **cross-platform corroboration, not a verified ABI match for River's iOS consumer**. Do not bulk-rewrite these responses from macOS headers. Use actual GET type/length/result observations, then inspect the matching iOS consumer if the layout remains uncertain. Patches0292/0295 deliberately leave these unverified getters unchanged;0295 corrects only the association-result enum supported by the actual scalar trace and prior iOS source.

Full follow-up kernel a6de705 through0293 built successfully; the final temporary image also includes0294 RPM reply matching.

Combined4d238b8 through0294 built, packaged and temporarily booted to responsive iOS. Its GET/SET trace proves keyed protected-packet exchange as described above. Settings connection and an actual lease remain unverified.

## Additional direct device checks on4d238b8

A read-only ioctl inspector returned `state=4` and `association_result=0`
after the keyed join, independently confirming the public-result mismatch.
The IPv4 inspector found en0 up/running with an address; this alone does not
prove a fresh DHCP lease. DNS resolution failed, and two current Apple IP
targets each returned `Network is unreachable` on sockets explicitly bound
and verified to en0. No route/DNS override was applied. Internet connectivity
is therefore not verified.0295 must be checked for result1, a Settings
checkmark and actual Wi-Fi-bound connectivity, rather than only packet-probe
or address presence. Helper captures remain private under workspace work/.
