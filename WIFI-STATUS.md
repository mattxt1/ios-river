# River Wi-Fi implementation audit and handoff

Updated 2026-11-13. Source baseline: applied `b90f2d1` kernel tree through patch0288. Patch0289 is integrated in236a880; the source audit did not access the phone or establish hardware reliability. Host regressions and the full kernel build pass. Device testing remains required.

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

The captured boot and join UDP logs for `236a880` contain **no SET request20 (ASSOCIATE), target lookup, authentication attempt or WPA attempt**. They show repeated scan request10 plus SET51 and SET383, and no keyed data-port success. The captures suggest a failure before the logged worker join path; bounded UDP logging can lose records and does not establish that every request was captured. The old source logged SET entry only, so GET preflight calls and return errors are absent from these logs; identifying which preflight operation prevents association requires another trace. A missing SET20 in these captures does not prove why the consumer declined to issue it.

Patch0292 replaces the unbounded SET-entry log with completion metadata for GET and SET: sequence number, operation, numeric type, input/output lengths, returned scalar and errno. The trace reads no user buffers, credentials, network names, BSSIDs or pointers. Atomic saturating counters cap successful calls at two and failures at four independently for each direction/type/result class, with a shared bounded bucket for out-of-range types. Repeated scans and unsupported vendor requests cannot exhaust another type's budget. It takes no engine/net locks and introduces no extra user-memory copy.

The exact user-visible warning has **not been traced to a verified private iOS error mapping**. Source does prove that the baseline reports generic association failure as value `16`, regardless of the real AP association status, and stale completions can publish that failure after a newer operation. Patch0289 prevents stale-result publication and clears the result on confirmed success. It deliberately retains existing error values `1` and `16`; it does not claim that either is a particular public 802.11 reason code or that the congestion warning is fixed.

Next device investigation should first capture patch0292 from boot through one Settings join, identify unsupported or malformed GET preflight replies and confirm whether SET20 is issued. Then correlate the exact Settings text with the join generation, reported Apple80211 association result, AP authentication/association status, WPA stage and link event timing. Change the error mapping only after confirming the consumer's enum/ABI or observing that correlation.

## Validation performed

Patch0292 applies cleanly after0289 against the integrated engine. The same actual-code state regressions compile and pass with0292 included:

```sh
python3 tools/test-river-wifi-state.py --kernel-source /absolute/path/to/pre0289/xnu --extra-patch xnu/patches/0292-river-wifi-ioctl-preflight-trace.patch
```

The `--extra-patch` option applies additional patches only to the temporary test copy. These tests preserve behavioral regression coverage; they do not claim to reproduce the private iOS consumer's preflight decisions. A separate host stress check of the extracted trace helper passed with eight concurrent threads and 800,000 calls: the shared type emitted exactly two success and four failure records, SET retained an independent budget, a later association type retained its budget, and negative/out-of-range types stayed bounded.


Run from the repository, with a kernel source tree containing patches through0288 and excluding0289:

```sh
python3 tools/test-river-wifi-state.py --kernel-source /absolute/path/to/applied/xnu
```

The runner applies0289 to copies of the three source files in a temporary directory and compiles the actual modified Apple80211 engine, RX handler and worker transition helpers against deterministic hardware stubs. It does not alter the kernel source or repository index. The compiled regressions passed with `-Wall -Wextra -Werror` (unused static functions/variables allowed for extracted source).

Coverage includes stale completion and stale link-loss rejection; exact BSSID selection; success clearing a previous failure; real RX handler deferring all SMD transactions; one cleanup per owner; old cleanup skipping a newer BSS; automatic reconnect reopening data and UI state; disappearance/reappearance of a target; association and WPA failure followed by another retry; retry exhaustion; user disconnect and radio-off cancelling retries; a newer join arriving during the old WPA operation; cancellation injected between completion and data-port activation; and boot-profile success, reconnect, cancellation and supersession. Source assertions verify RX cannot call `scan_xact()`/DXE polling and that the outer worker processes connection service before queued network output.

## Still unresolved

- Device verification of patch0289, including reconnect under AP loss, wrong credentials, repeated Settings joins, radio toggles and UI error reporting.
- The fixed-size 32-entry BSS cache has no expiry/replacement policy. Target lookup/recovery uses cached records; fresh scan scheduling and channel changes are not solved by this patch.
- The UI target path synthesizes a WPA2 RSN choice and marks rates/security accepted. It needs capability validation comparable to the strict beacon-selection path. Current support remains narrow WPA2 PSK/CCMP; this is not WPA3/enterprise/roaming support.
- A stuck TX DMA ring still needs explicit, verified recovery. Clearing its stalled flag during a new join is insufficient.
- Suspend/resume, firmware-reset recovery, long-running packet loss and broader network compatibility need hardware coverage.
- No claim is made that this audit found every cause of the user's intermittent connection failures. Its fixes address demonstrated state-machine and RX reentrancy defects.

## ABI cautions for the next investigation

Comparison with the Apple-origin headers maintained by the [itlwm driver](https://github.com/OpenIntelWireless/itlwm/blob/master/include/Airport/apple80211_ioctl.h) identifies suspicious replies: current GET14 returns MAC bytes although the header names14 PHY_MODE; GET23 returns scalar20 although the header describes STATUS_DEV_NAME; several other getters use only `req_val` where that implementation uses versioned structures. This is **cross-platform corroboration, not a verified ABI match for River's iOS consumer**. Do not bulk-rewrite these responses from macOS headers. Use actual GET type/length/result observations, then inspect the matching iOS consumer if the layout remains uncertain. Patch0292 deliberately makes no speculative ABI changes.

Full follow-up kernel a6de705 through0293 built successfully. The0292 device trace is still pending; the final temporary image will also include0294 RPM reply matching.

Combined4d238b8 through0294 built and packaged successfully. Awaiting temporary boot and responsive iOS for the GET/SET preflight trace; no new Wi-Fi success is claimed.
