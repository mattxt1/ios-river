# River Wi-Fi implementation audit and handoff

Updated 2026-11-12. Source baseline: applied `b90f2d1` kernel tree through patch0288. Patch0289 is prepared for integration; the audit did not access the phone or establish hardware reliability. Host regressions pass. A full kernel build and device testing remain required.

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

## The reported frequency/congestion warning

The exact user-visible warning has **not been traced to a verified private iOS error mapping**. Source does prove that the baseline reports generic association failure as value `16`, regardless of the real AP association status, and stale completions can publish that failure after a newer operation. Patch0289 prevents stale-result publication and clears the result on confirmed success. It deliberately retains existing error values `1` and `16`; it does not claim that either is a particular public 802.11 reason code or that the congestion warning is fixed.

Next device investigation should correlate the exact Settings text with the join generation, reported Apple80211 association result, AP authentication/association status, WPA stage and link event timing. Change the error mapping only after confirming the consumer's enum/ABI or observing that correlation.

## Validation performed

Run from the repository, with a kernel source tree containing patches through0288 and excluding0289:

```sh
python3 tools/test-river-wifi-state.py --kernel-source /absolute/path/to/applied/xnu
```

The runner applies0289 to copies of the three source files in a temporary directory and compiles the actual modified Apple80211 engine, RX handler and worker transition helpers against deterministic hardware stubs. It does not alter the kernel source or repository index. The compiled regressions passed with `-Wall -Wextra -Werror` (unused static functions/variables allowed for extracted source).

Coverage includes stale completion and stale link-loss rejection; exact BSSID selection; success clearing a previous failure; real RX handler deferring all SMD transactions; one cleanup per owner; old cleanup skipping a newer BSS; automatic reconnect reopening data and UI state; disappearance/reappearance of a target; association and WPA failure followed by another retry; retry exhaustion; user disconnect and radio-off cancelling retries; a newer join arriving during the old WPA operation; cancellation injected between completion and data-port activation; and boot-profile success, reconnect, cancellation and supersession. Source assertions verify RX cannot call `scan_xact()`/DXE polling and that the outer worker processes connection service before queued network output.

## Still unresolved

- Full kernel compilation and device verification of patch0289, including reconnect under AP loss, wrong credentials, repeated Settings joins, radio toggles and UI error reporting.
- The fixed-size 32-entry BSS cache has no expiry/replacement policy. Target lookup/recovery uses cached records; fresh scan scheduling and channel changes are not solved by this patch.
- The UI target path synthesizes a WPA2 RSN choice and marks rates/security accepted. It needs capability validation comparable to the strict beacon-selection path. Current support remains narrow WPA2 PSK/CCMP; this is not WPA3/enterprise/roaming support.
- A stuck TX DMA ring still needs explicit, verified recovery. Clearing its stalled flag during a new join is insufficient.
- Suspend/resume, firmware-reset recovery, long-running packet loss and broader network compatibility need hardware coverage.
- No claim is made that this audit found every cause of the user's intermittent connection failures. Its fixes address demonstrated state-machine and RX reentrancy defects.
