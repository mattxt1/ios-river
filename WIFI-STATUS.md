# River Wi-Fi implementation audit and handoff

Updated 2026-11-13. Patch0295 built and temporarily booted in `bb395d6`; direct queries confirm state4/result1 after joining. The user subsequently confirmed a Settings checkmark after the existing DHCP helper was restarted with observational SC logging. Native UI completion is now observed; it is not yet a reliable automatic connection or verified Internet result. WPA/protected DHCP exchange, helper DHCP ACKs and an IPv4 address are observed. Earlier DNS and bound numeric connection probes failed; fresh resolver/route observations remain in progress. Patch0296 native link-event readiness is prepared but not yet device-validated. The audit agent did not access the phone; device observations were supplied by the main agent.

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

- Full bb395d6 build through0295 passed. Temporarily boot the prepared image; confirm successful GET21 result1, Settings checkmark and a real configured DHCP lease. Patch0292 device validation already proved WPA/protected-packet success. Reconnect under AP loss, wrong credentials, repeated Settings joins, radio toggles and UI error reporting remain unverified.
- Patch0297 prepares bounded fresh-scan cache/snapshot and directed missing-target discovery changes; hardware validation remains pending. General off-channel scanning while joined and roaming/channel changes remain outside this fix.
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

The bb395d6 image built, packaged and temporarily booted with a 1 MiB retained kernel log (577-byte command line). State4/result1 is verified; Settings still spins without a checkmark. Its join was outside the UDP capture interval, so the retained kernel log and fresh route/DNS probes are needed before claiming anything about its data/configuration outcome.


## Remaining IPv4/configuration audit after bb395d6

Source inspection of `phone-tools/river-net.c` found a separate configuration layer: the DHCP daemon waits for scalarSTATE4, obtains an ACK, assigns IPv4, optionally installs a default route only with `--default`, then publishes SystemConfiguration state. The `sc-setup` command has a hard-coded fallback IPv4 address matching the observed address; `sc_config()` advertises `ConfigMethod=Manual`. The subsequent device inspection disproves the initial persisted-Manual hypothesis: the actual preferences service has `ConfigMethod=DHCP`, and the existing `org.river.net` job runs `monitor en0 --default` with KeepAlive. Its log reports accepted DHCP ACKs. The helper source still labels its own service-state publication Manual; preferences and transient service state are separate evidence.

The helper currently writes global IPv4/DNS/service-order state, calls `addNetworkInterface`, and publishes active AirPort/link dictionaries. Those are existing workaround behaviors, not native kernel connection evidence. No new route/DNS override or manual-address change was made during this audit. Current IOKit publication uses a minimal synthetic controller/interface hierarchy; successful ioctl/WPA operation does not demonstrate a complete native IPConfiguration service lifecycle.

A new read-only `phone-tools/river-network-config-state.c` inspector is prepared and compiled for arm64 iOS16 with warnings treated as errors. It reports bounded IPv4 route entries and selected State/Setup IPv4/DNS/interface/link plus selected preferences IPv4/DNS/interface fields. It does not acquire/renew DHCP, publish SC state, change routes, or inspect AirPort credentials. Its output contains private network addresses and belongs in private validation captures. Root will deploy it and collect the existing helper job/log metadata. Patch0296 now addresses the independently source-proven missing native data-link notifications; it does not add a manual route or DNS configuration. Its effect on the missing service/routes remains to be tested.


The selected `bb395d6` route/SC inspection found an en0 default route but no en0 connected-subnet or local-host route, unlike UE0. It found interface IPv4 and Link/Active state, but no global IPv4 or matching service IPv4/DNS State dictionaries, despite the helper logging publication success. These absent service/global keys are not explained merely by a missing default route or a Manual preferences setting. The source ignores every `SCDynamicStoreSetValue` return, so its success message does not distinguish denied writes from later configd cleanup. Checked write results and selected immediate/end readbacks are the next useful diagnostic; no route/DNS repair is inferred.

Kernel source inspection confirms `in_ifinit()` normally installs the connected route after the interface address ioctl. River and the working USB interface have the same simple address-ioctl behavior; no address-ioctl defect is established. A separate demonstrated integration gap is the lack of native `KEV_DL_LINK_ON/OFF` notifications through `ifnet_event()` on Wi-Fi link transitions. Setting IFF_RUNNING does not itself emit these events. Adding generation-safe transition events may be appropriate, but its impact on the observed route/SC cleanup still needs validation.


## Patch0296: standard data-link transition notifications

Prepared0296 adds `KEV_DL_LINK_ON/OFF` through `ifnet_event()` in addition to the existing private Apple80211 events. XNU `ifnet_set_flags()` does not emit these physical-link events; the prior iOS-flame port explicitly sends them for configd/protocol observers. This is a standard interface-readiness correction, **not a verified fix for the missing connected route, SC service state or Internet access**.

Only the WCNSS worker drains transitions, after connection service has returned from Apple80211 generation and net-queue locks. It checks confirmed keyed/current-generation data readiness, suppresses duplicate steady-state events, closes an older notified generation before opening a newer one, and holds an additional ifnet reference across protocol callbacks. The payload starts at `KEV_MSG_HEADER_SIZE`, with exactly one `net_event_data` containing the interface's actual family, unit and base name. It does not assume `sizeof(kern_event_msg)` equals the payload offset.

Generation validation immediately before unlocked delivery is the notification's ordering point. A cancellation before that check suppresses UP; a cancellation racing or reentering from delivery is reconciled with OFF on a subsequent drain pass. Delivering callbacks without engine/net locks is necessary to permit protocol/consumer reentry. Each drain is bounded to four transitions; any further state changes wait for the next worker tick. Failed event delivery is logged without retry storms or changing the connection.

Actual-code host regressions pass with0289/0292/0295/0296 applied. New checks verify one ON after keyed activation, one OFF for deauthentication/disconnect/radio-off, no duplicate steady-state events, no events for failed/stale/cancelled joins, older-generation OFF before newer ON, balanced ifnet references, exact payload layout with a trailing-word header, and unlocked reentry/cancellation from the event stub. The original pre0295 regression mode also still passes. Full kernel build and device observation of0296 remain required.


## SC observer and DNS source follow-up

The original installed helper was backed up before the observer replacement. Its existing code-signing identity/entitlements and monitor job were preserved; the main agent restarted only that existing KeepAlive monitor. The observation-only change adds no new SC writes, routes or DNS settings. All existing SC writes returned accepted with zero reported errors. Global IPv4 was already absent at immediate and end readback; service IPv4/DNS were present at both points. A later route dump showed connected/local en0 routes now present, compared with their earlier absence. The user then confirmed the Settings checkmark. This sequence does not establish which timing/state change restored routes/UI, nor Internet access. The first DNS probe afterward still failed.

Apple's [dnsinfo header](https://github.com/apple-oss-distributions/configd/blob/main/dnsinfo/dnsinfo.h) defines resolver flag2 as requesting IPv4 A records; the scoped flag is0x1000. Existing `__FLAGS__=2` therefore is not a reason to remove flags or claim a scoped resolver. Apple's [IPMonitor source](https://github.com/apple-oss-distributions/configd/blob/main/Plugins/IPMonitor/ip_plugin.c) derives global IPv4 from its elected service and route list, removes it without those inputs, and filters IPv4 DNS servers when it considers IPv4 inactive. This corroborates a possible native recomputation of helper-written global state; it does not prove the exact iOS16 decision here. Global SC dictionary presence alone is not proof of a resolver published to mDNSResponder.

The read-only `phone-tools/river-dns-state.c` inspector now reports actual compiled DNS resolver counts, interface indexes, flags and nameserver addresses, plus opaque NWI accessor results for IPv4/IPv6/DNS flags and the general IPv4 interface list. No DNS query, acknowledgment or state write is issued. Resolver layouts follow Apple's20170629 packed ABI and assert96/48-byte structures; NWI uses published accessors without assuming private structure layout. Lists are bounded. It builds for arm64 iOS16 with warnings treated as errors. Device captures remain private; no new route, resolver or preferences workaround was introduced.


## Verified packet connectivity; remaining general DNS/primary selection

The post-observer numeric Apple HTTP probe succeeded with its socket explicitly bound to and verified on en0: HTTP200 and the expected response marker. This establishes working IP packet connectivity through Wi-Fi for that bound probe. It does not establish default hostname resolution or unbound application connectivity. The default `getaddrinfo()` probe still fails.

The compiled DNS/NWI snapshot explains the distinction: en0 has IPv4 and DNS flags, but the general IPv4 interface list is empty and global IPv4 reachability is zero. Default resolvers have no nameservers/A-query flags; an en0-scoped resolver has two nameservers and flags0x1002. Thus the existing SC Global/DNS dictionary is not equivalent to a usable general resolver. Changing `__FLAGS__=2` is unsupported: that bit correctly means A-record queries, while0x1000 denotes scoped.

Primary Apple IPMonitor source accepts `PrimaryRank=First` but combines service Setup/State and interface assertions using the more restrictive rank. Never scopes the default route; Scoped scopes all routes. A helper State/First write therefore cannot override a separate restrictive assertion. The selected-state inspector now includes whitelisted root service options and the iOS15 `SCNetworkInterfaceCopyRankAssertionInfo` SPI (rank, PID and process name) to inspect all live interface assertions. The simpler GetPrimaryRank SPI reports only its own object and would miss other processes' assertions. No assertion has been changed and no DNS/global-state overwrite was added. A focused repair remains contingent on this evidence.


The live assertion snapshot identifies one restrictive en0 assertion: rank3 (Never), owned by configd. The helper's service State/First does not override this interface assertion. This directly explains why bound Wi-Fi packet traffic can work while primary election and default DNS remain unavailable. The assertion's reason is not exposed by the rank-info SPI.

A complete search of the downloaded public Apple configd and bootp/IPConfiguration source found no production policy call site that sets interface rank Never: the public framework exposes the setter and IPMonitor consumes assertions, but the observed iOS policy owner is not identified by that source. A closed iOS configd plugin is a possibility, not an established diagnosis. No attempt was made to kill configd, clear another owner's assertion, or repeatedly overwrite its global output. Patch0296's standard link events may cause the owner to reevaluate state, but that effect cannot be claimed before device testing. Mapping service UUIDs to interface entities and inspecting bounded owner logs is the remaining evidence path.

Full kernel dbf26b6 through0296 built successfully. Its temporary image is
packaged and verified (26,697,728 bytes,577-byte command line,1MiB retained
log). Device event/rank/DNS validation is pending. The latest phone result
remains Settings checkmark and Wi-Fi-bound numeric HTTP success on bb395d6,
with ordinary DNS unresolved; no rank assertion has been forcibly cleared.


## dbf26b6 discovery regression and patch0297

After temporary booting0296, the user's usual target was absent from Settings and Other-name entry could not find it. The retained log shows84 completed physical scans with the BSS count stuck at32,6482 received beacons and zero reported bad RX frames. Source proves the lifetime cache accepts only its first32 distinct BSSIDs; `clear_scan()` was never called by the physical scan. Repeating a scan therefore cannot admit later-channel/new APs after crowding. This is a source-proven discovery failure, not evidence that the user's AP stopped transmitting.

Prepared0297 uses fixed64-entry working and64-entry completed-result arrays. Each actual physical scan clears the working cache, strongest-RSSI replacement admits later strong APs while evicting stale entries first, and a requested SSID/BSSID is retained despite unrelated crowding. Entries older than30 seconds are rejected for lookup/publication. GET11 enumerates the completed snapshot, independent of live RX updates; each completed physical scan publishes a new snapshot and resets enumeration. As with the existing API, multiple simultaneous consumers share an enumeration cursor; the fix does not claim independent per-client scan sessions.

Scan request10 now validates and copies only its fixed version1/name prefix, checks SSID length, and uses that name in probe requests. A missing Other-name join performs one bounded11-channel directed pass, publishes its scan results, then rechecks request ownership and exact BSSID before association. The wildcard probe remains36 bytes; a maximum32-byte SSID uses68 bytes. Actual physical scan completion preserves a scan request queued while that pass was running. Off-channel physical scans remain withheld while the keyed connection is live. Boot-profile reload is restricted to initial startup rather than overwriting a UI-directed target.

Hidden beacons do not erase a name learned for the same BSSID; no name is borrowed from another BSSID. Complete IE validation precedes cache mutation, and stored IEs end on an element boundary. RSN/rate data remain those of the received record. The valid existing self-station is reused on later scans rather than sending duplicate ADD_STA_SELF. The old log saying probes were withheld after duplicate status31 was misleading: source retained the previously valid self ID, and924 probe submissions/DMA completions and270 probe responses confirm TX continued.0297 does not reinterpret duplicate status31 as success or reset a valid station ID.

Actual-code host regressions pass with0289/0292/0295/0296/0297 applied and in the original baseline mode. Coverage includes100 early crowded records plus a strong late-channel AP, weak directed-target retention, stale-first eviction, disappearance on refresh,30-second aging, snapshot stability while RX updates the next scan, same-BSSID hidden name preservation, malformed IE rejection, strongest/exact-BSSID selection, wildcard/max-name probe byte layout, malformed scan prefix/version/name length, a request arriving during a physical pass, one-pass missing-target discovery, cancellation/new join during that pass and no stale completion. Full0297 kernel build/device validation remains pending; no route, DNS, rank or credentials workaround was added.

The three other Scoped service UUIDs in the rank dump map to IPv6 utun interfaces, not competing conventional Wi-Fi services. The helper's en0 service remains First, and configd's interface Never assertion is the remaining primary-election evidence. The discovery repair does not claim to clear that policy.


Discovery limitations remain explicit: the bounded list retains at most64 records with strongest-RSSI selection and directed-target priority; it cannot show every AP in an arbitrarily dense environment. Radio scanning still covers2.4GHz channels1–11 only, with no5GHz or channels12–14 support. A missing target gets at most one pass, which cannot guarantee that an AP transmits/responds during the bounded dwell. Named probes and same-BSSID name preservation improve this path; they do not establish general hidden-SSID compatibility. GET11 retains its one shared ioctl enumeration cursor.

## Patch0297 build and boot handoff

The full c3d9af8 kernel build passed. Its temporary image passed all payload,
size and command-line checks and was accepted by fastboot on River. Responsive
iOS confirmation and actual scan/join validation are pending. No claim is
made yet that the user's AP appears or that native primary selection/DNS is
fixed. The image preserves the original verified base network helper; the
earlier diagnostic observer is not embedded. See NOTES.md for hashes and
exact artifact paths.


## c3d9af8 fresh-scan and Apple registration-helper inspection

Patch0297 built, packaged and temporarily booted to responsive iOS in c3d9af8. Fresh scan counts now vary13–24 instead of remaining at32, verifying that completed scans refresh the bounded discovery set. The user's target visibility and a new join on this image remain to be confirmed; varying counts alone do not prove the desired AP was received.

Read-only import/disassembly inspection of the device's existing `addNetworkInterface` binary establishes its rank decision without executing it. At virtual addresses0x100002bd8–0x100002c30 it reads `NetworkInterfaceFlags` from the controller parent: bit0x20000000 selects service rank3 (Never); an absent/clear property selects rank0 (Default). At0x100003080–0x1000030d0 it compares the existing service rank and calls `SCNetworkServiceSetPrimaryRank` only when the desired rank differs. It also checks recursive `HiddenInterface` and the `InterfaceRole` string `CarPlay` to skip configuration.

Current RiverWLAN source publishes none of those properties, so this helper's observed path selects Default rather than Never. It imports service rank APIs, not `SCNetworkInterfaceSetPrimaryRank`; this does not explain the separately observed configd-owned interface assertion. The minimal synthetic IOKit hierarchy remains a possible broader integration limitation, but this binary supplies no basis for inventing new property bits or clearing another owner's assertion. Device binary and disassembly remain outside Git in private/intermediate workspace files.


## c3d9af8 target found; two join attempts and bounded M1 audit

The user now sees the usual target under My Networks, confirming the refreshed discovery path received it. The first join prompted for a password and showed the previous alert; the second prompted again and then showed a spinner without an error. Private retained logs establish first-attempt AP authentication/association status0 followed by four M1/M2 exchanges, no received M3, and AP disconnect reason15. The second attempt completed M1/M3, key installation, M4 and the protected DHCP OFFER probe. Direct scalar state is4/result1. Native IPConfiguration remains incomplete on this boot: configd still owns en0 Never, NWI has no en0/general IPv4 state, and no scoped resolver is published in that snapshot. No DNS/Internet success is claimed for c3d9af8.

Source inspection finds no demonstrated cross-generation PMK/SNonce carryover: each worker attempt clears WPA state and copies the current request PMK; unchanged ANonce reuses the same SNonce/PTK, lower replay counters for that ANonce are rejected, and M2 echoes the received replay. A new ANonce before key installation derives fresh SNonce/PTK. The existing comment saying only identical nonce/replay is accepted is stricter than the implementation, which allows an increased replay counter and a new ANonce before keys. The captured logs contain no nonce/replay-change metadata, so the first failure cannot be attributed to that policy. DMA completion alone is not an AP acknowledgment or proof that the AP accepted M2's MIC. Wrong credentials, packet delivery and AP handshake behavior remain hypotheses; no request for secrets or unsupported explanation is warranted.

The standard DL event payload remains source-correct: exact event-data offset/size, interface family/base-name/unit, worker delivery outside locks and success-based link readiness. Included code and absence of a logged event error do not independently prove native consumers completed configuration or cleared their rank policy. No further patch is justified by these captures; review of the existing helper's process/log and native configuration owner remains the next bounded evidence path.


## Audio continuation passive snapshot on1334e8c

The next candidate0303 batches audio mapping/session/AFE tests. No additional
Wi-Fi patch is justified by the passive current-boot snapshot: configd still
owns en0 Never, and no en0 IPv4 routes/NWI/general resolver are present, but
this is not a confirmed joined-state test. Retained kernel logs show refreshed
scans. A bounded scalar state helper timed out while shell-only SSH, APR
status and kernel-log reads worked; no specific kernel lock defect is proven.
Evidence is private under the2026-11-13 continuation work/pcm-mapping folder.
No rank, resolver, route or credentials workaround was applied. Wi-Fi remains
secondary to audio, with native primary election/DNS still unresolved.


## Fresh join: brief checkmark, then spinner on1334e8c

During the0303 audio build the user tried a fresh join. The retained log
proves successful WPA key installation/M4 and protected DHCP probe, with
public GET21=result1. The user observed a brief checkmark then a spinner,
without an alert. The immediate route/SC/DNS snapshot has en0 default route
and IPv4/Link state, but lacks connected/local routes, service IPv4/DNS,
usable compiled DNS and NWI presence; configd retains its Never assertion.
No later link-loss/deauth appears in that captured interval. This supports
investigating native configuration after keyed association; it does not
prove which configd policy/cleanup caused the failure. Captures stay private.
The0303 image passes full build and packaging, batching audio prerequisite
tests with existing Wi-Fi patches; it introduces no rank/DNS workaround.


## Bounded monitor reader prepared alongside0304

Reading the existing monitor log through a bounded SSH shell command timed
out without output. No daemon was restarted. A small read-only native helper
now reports the installed river-net hash, fixed job plist and last64KiB of
its log, with stage markers and regular-file/size checks. It is included in
the0304 image without another kernel build. Execution/evidence remain pending;
no configuration or rank workaround is added.


The0304 image now built/packaged and temporarily booted with the native
reader included. Its execution is pending responsive-home confirmation.
Expected embedded baseline river-net is71,728bytes/SHA256
807a0190cac9c3dbb3ce685418f0235af95dc025b9192ba0275fa93dbb7b44ba.
Compare the new reader's live hash before assuming the observer or source
version is running. No Wi-Fi connectivity/configuration fix is claimed.


## Native monitor diagnostics on0304

The reader ran successfully and confirmed the baseline embedded helper hash
and monitor en0 --default KeepAlive job. Historical log entries include DHCP
ACK/renewal success, but are untimestamped and cannot establish current-boot
connectivity. Current scalar state0/result0 is unassociated; fresh join/UI
evidence was requested while the audio readback correction built.
Some host SSH timeouts were traced to an iCloud-offloaded approved key before
connection. In-place hydration fixed that case. Do not infer a kernel lock
problem from those timeouts. No rank/route/DNS override was applied.


The user subsequently reported checkmark then spinner on0304 and returned
the phone to fastboot. That reply was later than the passive unassociated
snapshot; it cannot be treated as an immediate post-event capture.0305 is
temporarily booted with home confirmation pending. The persistent log reader
can compare new bytes against the earlier16,086byte snapshot, while retaining
the lack of native per-entry timestamps as a limitation.

##2026-11-14 audio-boot observation window

The0304 post-join log body (16,086bytes) exactly prefixes the0305 reader's
16,788bytes. The702new bytes contain associatedSTATE4, accepted DHCP ACK,
existing service rank0 and helper SC DNS/IPv4 publication, then a monitor
startup. This bounds the new log window but supplies no entry timestamps or
immediate native DNS snapshot. It spans reboot and does not prove working
Internet or resolve the latest user report, checkmark then spinner.
0306 retains the read-only reader for combined audio/Wi-Fi observations.

##0306 same-boot associated follow-up (2026-11-14)

0306 Wi-Fi follow-up now independently confirms state4/result1 (earlier
baseline0/0). Native route dump contains en0 default route but no connected
local-subnet/own-address route; configd still asserts interface rank3Never.
NWI en0 and general IPv4 remain absent. Global SC DNS has ServerAddresses,
but native dnsinfo has seven default resolvers with zero nameservers/A/AAAA
and no scoped/service resolvers. Interface IPv4 and Link Active are present;
no State service IPv4 entity appears in the enumerated native snapshot.
Evidence work/pcm-stream/{wifi-followup,network-followup,dns-followup,
wifi-monitor-followup}-98dcdb1.txt. This is a fresh same-boot associated
snapshot. The user subsequently confirmed Settings showed a spinner. No workaround applied.
Next image ready; fastboot requested after user's Wi-Fi attempt.

##0315 native Wi-Fi follow-up (2026-11-14)

Before new join request, current scalar state1/association_result0. Native
configuration has no en0 IPv4 route, configdPID35 interface rank3(Never),
NWIen0 absent and compiled DNS seven default resolver records with zero
nameservers/A/AAAA and no scoped/service resolvers. Retained monitor log's
DHCP/publication entries are historical; do not assign them to this boot.
Requested fresh user join while0316 audio correction builds. No Wi-Fi code
or route/rank/DNS overrides applied. Private evidence work/codec-route/
wifi-{native,config,dns}-c305b28.txt and wifi-monitor-c305b28.txt.

##0315 fresh spinner join captured before reboot (2026-11-14)

User confirmed post-audio iOS responsive (reply tes), then reported Wi-Fi
spinner/no error and requested logs before fastboot. All five USBSSH reads
completed exit0: scalar57B,monitor20030B,configuration7811B,DNS752B,
kernel262144B. Private evidence work/codec-route/join-*-c305b28.txt,
SHA/size manifest join-capture-manifest.json. No further active audio controls.
Current scalar en0state4/association_result1; new monitor growth records
associated->DHCPACK/address config and helper service/systemDNS publication.
Kernel records PTK/GTK installed once/M4DMA and protected DHCPOFFER probe.
Native table contains en0 default route but lacks connected/localIPv4route;
configdPID35 still owns interface rank3(Never). NWIen0 absent/generalIPv4
empty. Seven default DNS records have zero nameservers/A/AAAA, no scoped or
service resolvers. This supports failure after keyed association in native
network configuration, not successful usable Wi-Fi. It does not identify
private configd policy owner. No rank/route/DNS override or Wi-Fi patch.
User notified capture complete and can move to fastboot for verified0316.

##0318 restores publication observation to combined image

Installed daemon fingerprint lacks existing tracked SCwrite/readback logging.
0318 embeds recompiled tracked observer with same entitlements and source
behavior; no live restart or network override. Verify installed hash after
boot before fresh join. See active NOTES.md for hashes and private evidence.

##0318 spinner with verified observer

Fresh join state4/result1; all writes accepted, globalIPv4 present immediately
and absent by observer end; serviceIPv4/DNS present at end but absent in later
native snapshot. Configd rankNever, no connected/local route/NWI/resolvers.
This localizes disappearance after acceptance; policy owner unresolved.
Kernel log partial/SSH timeout, other four diagnostic reads complete.
No network override applied; private work/codec-pcm-order/join-* captures.

##0320 kernel address observation

Combined image preserves verified publication observer and adds read-only
getifaddrs to river-network-config-state: kernel flags/IPv4/mask for en0/ue0
before route/SC state. Compile/sign and packed bytes verified; no configuration
behavior change or native setter call. Device verification/fresh join pending.

##0320 delayed Wi-Fi checkmark: scoped connectivity verified (2026-11-14)

User reported blue Settings checkmark after phone idle for a while. All five
read-only USB captures completed exit0: scalar57B/config10048B/DNS910B/
monitor46650B/kernel262144B. State4/result1. New getifaddrs proves kernel
en0IPv4 assigned,UP/RUNNING, connected/local/gateway routes now present.
ServiceIPv4 and serviceDNS persist in this snapshot; globalDNS has servers.
NWIen0 now flags0x5 IPv4=1/DNS=1 and scoped resolver ifindex8,A=1,2servers,
reach0x20002. Compared with earlier spinner evidence, this is real partial
native integration, not only a UI change. Current32 pre-checkmark kernel-address
reads had failed before auth due offloaded key; don't invent that comparison.

Remaining: configdPID35 still owns interface primary rank3Never; generalNWI
IPv4 list empty and seven default resolvers have0nameservers/A/AAAA. Bounded
existing probes explicitly bound toen0 passed gatewayTCP/HTTP302 and Internet
numericTCP443; normal getaddrinfo(captive.apple.com) failed EAI_NONAME.
Thus Wi-Fi data/Internet IP traffic works with interface scoping, but native
default-network/DNS selection remains broken. Checkmark is not proof of
normal Internet access. No configuration writes/restarts/overrides performed.

Monitor current tail contains multiple automatic DHCP lease renewals accepted
with1hour leases/T1~26-28min, each invoking existing sc_config publication.
Initial acquisition calls addr_config/default_route/sc_config; renew only
calls sc_config. Last renew logs all writes accepted, globalIPv4 immediate1/
end0, serviceIPv4/DNS present at both checkpoints. Current snapshot has
persisting service state and scoped resolver. Likely delayed checkmark is
renewal/republication and native timing/race settling after initial association;
not a proven exact renewal/owner cause: logs lack timestamps for UI transition.
No new build or runtime change preceded the observation, only local host
storage migration. Kernel uptime sampled about20437s. Source no user-triggered
renew or IPConfiguration setter run. Need timestamped state transitions to
pinpoint which publication/route event triggers the checkmark. Keep working
on primary rank/default NWI/resolver service lifetime, not WPA retries.
Private evidence work/wifi-checkmark/*-checkmark.txt and capture-manifest.json;
raw network addresses/logs outside Git. Preserve this useful live0320 state
until next audio image is concrete and user ready; no spontaneous reboot.
