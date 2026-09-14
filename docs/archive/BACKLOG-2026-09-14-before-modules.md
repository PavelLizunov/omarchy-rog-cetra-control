# Archived backlog before modularization

This is a historical record, preserved verbatim below except for this notice.
Use ../../BACKLOG.md for active status and ../../MODULES.md for current paths.
Old line references, counts and pending-parent statements describe their dates.

> Authoritative backlog for release blockers, engineering debt, missing tests,
> and unresolved protocol research. Read this file after `AGENTS.md` and
> `HANDBOOK.md`, before changing runtime behavior.

Historical audit: 2026-09-05
Audited commit: `094a649c1a0851bfe78baa362836e5a1fe36ee50`
Current source/documentation review: 2026-09-08, uncommitted working tree;
see [REVIEW-2026-09-08.md](../../REVIEW-2026-09-08.md). Fresh aggregate verification is
pending with the parent agent, not established by this documentation review.

## Status

Publication preparation (2026-09-14): see RELEASE.md for evidence-backed P0
reconciliation. Source-only setup passed two isolated runs with unchanged helper
hashes/mtimes on the second run; discovery and host controls were mocked. The
remaining live acceptance is not waived by that result. Candidate version is
1.6.0; v1.5.0 remains a historical tag.

2026-09-14 follow-up: manual Request call mode and M/Ь removed; legacy saved
alwaysCallContext is ignored. Automatic detection remains. Live ANC logs show
valid level changes arriving 4–8 seconds after a write, beyond the former
three-second UI deadline. Pending settings now allow 48 ticks (nominal 12 seconds)
and clear timeout feedback on matching late readback, without retrying writes.
This supersedes the historical twelve-tick descriptions below.

2026-09-14 implementation: opt-in auto-theme updates require a current-session
explicit colored apply; saved opt-in alone sends nothing. One shared 350 ms timer
coalesces theme changes and Off/Cycle are preserved. This extends P0.2's explicit
authorization policy without adding startup writes. Call request is labeled
experimental and no longer promises confirmed tap assignment. Protocol/document
discrepancies are clarified, but official Lighting Off/duplicate-commit and EQ
mapping verification remain open. This does not close unrelated release blockers.

2026-09-13 follow-up: ten interface catalogs and a header picker are now bundled.
User follow-up supersedes the earlier settings acceptance: the widget injection
can lag too. The corrected Qt fixture reproduces the second rollback. Preferences
now use FileView readback of the own shell.json entry, with accepted changes
protected from older completions until confirmation or a three-second reload.
Auto-pause UI and P/З are hidden pending verified USB behavior.
The settings snapshot-order bug, stale detection on override transitions,
language focus restoration and constrained control labels have Qt/JS regression
coverage. The aggregate suite passes, including real Qt settings-order checks and
420 constrained label layouts. Full live pointer/keyboard acceptance remains
pending. USB Play/Pause suppression and proximity-event delivery
remain unresolved; the dated RESEARCH sections separate observations from claims.
This update does not close the P0 release blockers below.

The audited commit passed its then-current automated checks, but the project is
**not ready for a new release or Marketplace submission**. Those historical
results do not verify the work in progress. The existing tests did not cover
several behavioral and lifecycle defects listed below.

Release inconsistency recorded at the historical audit (remote tag and branch
distance were not rechecked on 2026-09-08):

- `manifest.json` reports version `1.5.0`.
- GitHub tag `v1.5.0` points to `2882cd84fd92ca6aa8e64dd0350e78227cd9c15b`.
- Current `main` is seven commits ahead of that tag while retaining version
  `1.5.0`.
- Do not move or overwrite the existing release tag. Use a new version only
  after the release blockers are resolved.

Current source status: P0.3 is implemented and verified offline according to
supplied results; parent live/aggregate acceptance remains pending. This does
not close all P0 items. The microphone unknown-state contract,
explicit-session lighting guard, shared service, helper-error clearing, and
presence freshness gate are implemented, but do not close all release work.
RGB controls and ten interface catalogs are present. Full
keyboard/rendered acceptance, teardown guarantees, remaining telemetry hardening,
and live acceptance remain open. Dated implementation updates below supersede the original
problem descriptions; historical passing tests are not current-tree results.

## P0: Release Blockers

Research update (2026-09-07): stable-owner side-separated user trials are now
documented in RESEARCH.md. Effective mute was lost across left-side transitions
without a new tap; right-only trials preserved it. Presence/charging byte-5 side
correlations were initially recorded without verified bit encoding or byte 6.
The initial official HAL download failed at TLS; a later IPv4/HTTP1.1/TLS1.2
download succeeded with matching ZIP and DLL hashes. RESEARCH.md now records
verified bit masks 0x01/0x10 and the byte-6 Case charging slot. Passive nullable
JSON decoding, strict invalid domains, raw preservation and 30-second expiry are
implemented with 3916 parser events and an owner-loop cache/client expiry test.
That initial implementation added no queries or UI indicators. The current
2026-09-08 owner sends verified presence request `01` on receiver open and every
10 seconds; charging `08` is still passive. Both decoded families expire after
30 seconds without a matching response. The UI now displays their states and
gates controls on fresh presence after the first report; C `connected` remains
battery-derived. Raw fields remain historical after expiry. Freshness TTL is
plugin policy; none of these fields is absolute microphone mute readback.

### P0.1 Remove the false absolute microphone state

Partial mitigation (2026-09-05): QML no longer consumes `mic_live`, displays
Live/Muted, or sends manual `mic_state` resync commands. The panel states that
mute is unknown and the headset voice prompt is authoritative. UI source guards
were added to `tests/run.sh`.

Implementation verified offline (2026-09-06): daemon `mic_live`, gesture inversion,
reconnect Live resets, and the `mic_state` handler are removed. JSON publishes
`microphone_state: "unknown"`; `tap_seq` counts observed reports and saturates at
INT_MAX. Optional byte 7 is guarded. The 101-event microphone contract suite
passes both strict compilation and ASan/UBSan; historical HEAD fails the contract.
Independent source review found no remaining issue in this narrow change.
Documentation no longer claims implemented ADC mute detection. Deployment and
physical gesture/case acceptance are separate; earbuds are charging, so no new
physical mute/readback claim is made. Other P0 blockers remain open.

Affected areas from the audit (not a claim that all remain unchanged):

- `cetra-watch.c`: `mic_live`, `tap_seq`, `mic_state`, gesture inversion, and
  out-of-case reset paths.
- `Cetra.qml`: `micLive`, the Live/Muted card, bar indicator, tooltip, and manual
  resynchronization click.
- `README.md`, `CHANGELOG.md`, `HANDBOOK.md`: claims about hardware-synchronized
  Live/Muted and ADC verification.

Problem at audit:

- `cc 70 .. 01 01` is an edge notification, not an absolute mute-state report.
- The daemon initializes `mic_live=true` and toggles it locally after selected
  gestures.
- A missed, duplicated, reordered, or context-misclassified gesture reverses the
  displayed state.
- Starting the daemon while the headset is already muted also starts in the
  wrong phase.
- Clicking the microphone card only changes software bookkeeping; it does not
  read or change the headset state.
- Production code does not implement the PCM/ADC silence-gating claimed by the
  earlier public documentation. PCM silence observations can reflect graph
  mute, gating, routing, processing, or capture failure and are not validated
  absolute headset mute readback.

Risk:

- The UI can show `Muted` while the microphone is physically live, which is a
  privacy-sensitive failure.

Required direction:

- Until a reproducible absolute hardware readback exists, expose only facts such
  as `call gesture active`, `right tap observed`, and `state unknown`.
- Do not expose persistent Live/Muted as authoritative.
- Do not add software or ALSA mute disguised as headset-native mute.
- If PCM analysis is revisited, it must not create a competing audio reader or
  interfere with Discord, Voxtype, EasyEffects, or other capture clients.

Acceptance criteria:

- No persistent `mic_live` state derived solely from edge counting.
- No `mic_state live|muted` IPC command.
- JSON publishes `microphone_state: "unknown"`; `tap_seq` is only a count of
  observed in-call right-earbud single-tap reports, never a mute bit or parity.
- UI and docs clearly say that the headset voice prompt is authoritative.
- Tests prove that left-earbud gestures, media gestures, duplicated events, and
  daemon restarts cannot produce a false absolute mute claim.
- Short gesture reports, including 7-byte taps, do not access missing byte 7;
  regression tests cover this boundary.

### P0.2 Stop implicit Lighting writes during startup and reconnect

Implemented in current source (2026-09-08), acceptance pending: startup lighting
is unknown and `lighting_desired_valid` becomes true only after a complete
explicit transmission. Only that successful session preference is replayed on
case/USB reconnect; restart forgets it. Saved UI RGB/theme preferences do not
authorize a write. `tests/lighting-safety/` and `tests/lighting-color/` cover the
intended boundaries, but this review did not run them. The following problem is
the original audit finding, not the current default behavior.

Affected code:

- `cetra-watch.c`: default/reset lighting state and the
  `!was_connected && state.connected` reconnect path.

Problem:

- The daemon defaults lighting to `off` and automatically calls `set_lighting()`
  on the first connection and every out-of-case transition.
- This writes and commits an onboard setting even when the user did not request
  a lighting change in the current session.
- Restarting the shell or daemon can therefore modify hardware state.

Required direction:

- Track `lighting_desired_valid` separately from `lighting_desired`.
- Set that flag only after an explicit, successfully transmitted user command or
  after a confirmed persisted preference is intentionally loaded.
- Reapply lighting after the case only when desired state is valid.

Acceptance criteria:

- Starting or reconnecting the daemon sends no lighting reports unless a valid
  desired state exists.
- A test verifies that the first battery response cannot trigger a lighting
  write by default.

### P0.3 Replace fabricated settings with unknown/confirmed state

Implementation verified offline (2026-09-08, supplied results, not rerun in this
documentation pass): settings start/reset unknown, validate report domains, and
expire 30 seconds after valid readback. JSON uses nullable ANC level/Adaptive/
proximity and `voice_prompt: "unknown"`; confirmed voice tokens stay English
protocol tokens (`english`, `chinese`, `sound`), independent of UI catalogs.
Invalid reports, receiver loss, and unavailable earbuds invalidate observations;
reported all-earbud absence vetoes late replies and polling until new presence.
The UI additionally gates settings on fresh presence after its first observation.

The monotonic round robin sends verified `2b/2c/28/26`, one periodic request every
2500 ms, each field once per nominal 10-second cycle at most. The first eligible
query is `2b`; the next-query deadline survives absence/reconnect, preventing
bursts. Owner stalls can delay it; no ASUS frequency or tolerance guarantee is
claimed. Explicit writes invalidate the affected readback and request it again.
Shared pending settings do not optimistically update selections; matching
readback clears them, otherwise twelve 250 ms scheduler ticks show unconfirmed
feedback. This is not a wall-clock deadline; stalled UI delivery delays expiry.
Unknown boolean actions request On; manual ANC selection disables Adaptive when
it is On or Unknown. Supplied settings coverage is 10607 parser events and 36
owner scenarios. Parent must provide current aggregate and live evidence before
acceptance; this is not closure of the entire P0 section. The problem below is
the historical audit finding, superseded by this implementation update.

Affected code:

- `cetra-watch.c`: defaults and polling schedule for ANC level, Adaptive ANC,
  voice prompt, proximity, and lighting.
- `Cetra.qml`: optimistic defaults and active button states.

Problem at audit:

- The daemon parses `0x2b`, `0x2c`, `0x28`, and `0x26`, but its periodic settings
  polling covers only mode (`0x25`), alongside battery (`0x07`) and presence (`01`).
- Before a real response it publishes `High`, `Adaptive off`, `English`, and
  `Proximity on` as though they were hardware-confirmed.
- Disconnect resets those values to the same fabricated defaults.
- Lighting has no confirmed readback and must be represented as desired/last
  command, not hardware-confirmed state.

Required direction:

- Introduce explicit `unknown`, `desired`, `confirmed`, and `stale` semantics.
- Restore a bounded query schedule for verified readback opcodes only after
  confirming that the receiver tolerates it reliably.
- Keep lighting as desired state unless a real readback is discovered.

Acceptance criteria:

- UI never highlights a setting before a valid response or an explicitly
  labelled desired state.
- Disconnect/staleness visibly returns fields to unknown rather than invented
  defaults.

### P0.4 Fix watcher/call-context restart synchronization

Implementation verified offline (2026-09-06): stopped writes no longer change
sent intent; onStarted forces reconciliation. Extracted production QML tests
cover active restart and media reconciliation. Physical call controls after
restart still require charged hardware verification.

Affected code:

- `Cetra.qml`: `updateCallContext()`, `deviceWatchProc.onExited`, restart timer,
  and call detector.

Problem:

- `requestedCallContextActive` is changed before writing to `deviceWatchProc`.
- If the watcher is stopped, `Process.write()` is effectively lost while local
  requested state still changes.
- After restart, equality checks can suppress the required `call on` command.

Required direction:

- Do not update requested/sent state unless the process is running and the write
  is actually attempted.
- On watcher start, explicitly reconcile desired call context.
- Distinguish desired, sent, and hardware-reported call context.

Acceptance criteria:

- Restarting the watcher during an active communication capture re-sends
  `call on` and restores call controls.
- A regression test covers this transition.

### P0.5 Correct release and documentation truthfulness

Affected files:

- `README.md`, `CHANGELOG.md`, `HANDBOOK.md`, `RESEARCH.md`, `manifest.json`, and
  GitHub release metadata.

Problem at audit:

- At audit, public claims said ADC silence gating and absolute hardware-synchronized
  mute status were implemented, while production code performed edge counting.
- `CHANGELOG.md` claimed an extended multi-phase query schedule absent in the
  audited code; the current bounded settings scheduler is a later implementation.
- Security/privacy documentation does not disclose persistent telemetry or all
  runtime dependencies and hardware writes.

Documentation update (2026-09-08): README and CHANGELOG now distinguish current
presence polling from passive charging, explicit RGB apply from saved selection,
interface catalogs from headset voice languages, and historical evidence from
pending acceptance. README discloses persistent logs, rotation above 5 MiB to one
`.old` backup, normal-operation logging without an opt-out, saved preferences,
runtime dependencies, and installation network access. The false no-persistent-
data statement and unqualified historical extended-query claim are corrected.
The current settings schedule, tick-based pending feedback, strict IPC domains,
owner-only log hardening and its remaining path/fallback limits are documented.
The unsupported fixture-prefixed shell restart was replaced with an offline
`cetra-status` test-helper example, not a UI-preview command.
HANDBOOK and historical RESEARCH runtime summaries still need reconciliation
with the current service/query architecture; they were outside this docs-only
edit scope. Release metadata and whole-tree acceptance remain open.

Required direction:

- Make `RESEARCH.md` the source of truth for verified facts.
- Remove or qualify all unproven claims.
- Keep runtime `bash`, `pactl`, `jq`, GNU `timeout` (coreutils), and `hidapi`
  separate from test-only Python 3/Node.js; disclose telemetry, log rotation,
  saved preferences, and every supported hardware write.
- Bump to a new release version after blockers are fixed; do not reuse `1.5.0`.

## P1: High-Priority Runtime Correctness

### P1.1 Handle helper error states in QML

Implemented in current source (2026-09-08), acceptance pending:
`CetraService.qml:53-85,122-128` forwards valid JSON error statuses and clears
telemetry/pending mode state. This fixes the original forwarding defect below.
Malformed JSON is still ignored and C open failures are not distinguished (P2.6).
Settings validity and pending clearing are now implemented under P0.3, with
parent live acceptance pending.

Affected code: `Cetra.qml` `applyStatus()` and `applyDeviceState()`.

- `applyDeviceState()` currently forwards only `status == ok`.
- `permission-denied`, `protocol-error`, `timeout`, `busy`, and
  `helper-missing` are dropped, leaving stale controls enabled.
- Forward every valid JSON status and clear/disable stale device controls on
  error.

### P1.2 Make call detector failures safe

Implementation verified offline (2026-09-06): a common three-nonpositive-result
budget also bounds alternating inactive/unknown results. GNU timeout kills the
entire detector group after two seconds; the four-second QML watchdog triggers
the same expiry handler via SIGALRM. Four isolated process-tree scenarios pass,
including descendants ignoring TERM. Results are counted only once per poll.

Affected code: `Cetra.qml` call detector process and debounce.

- `unknown` leaves the previous detected state unchanged.
- A failed or hung `pactl`/`jq` process can leave call mode active indefinitely.
- Add a process timeout, reset debounce on unknown, and conservatively return to
  media mode after a bounded failure threshold.

### P1.3 Narrow communication detection semantics

Affected code: `Cetra.qml` `pactl | jq` filter.

- Application-name matching can classify generic browser recording as a call.
- Prefer explicit `media.role=communication|phone` and call-specific metadata.
- Use application names only as a carefully documented fallback.
- UI should say `Communication capture detected`, not claim a proven call when
  metadata is ambiguous.

### P1.4 Persist `alwaysCallContext` correctly

Implementation verified offline (2026-09-06): readonly settings binding plus
updateEntryInline with merged settings. Extracted QML tests verify preservation
of neighboring settings and override priority; real shell reload acceptance is
still separate.

Affected code: `Cetra.qml` `alwaysCallContext` and `setAlwaysCallContext()`.

- Assigning directly to a property initially bound to `setting(...)` destroys
  the binding.
- The value is not written with `bar.shell.updateEntryInline()` and is lost on
  reload.
- Make it a readonly setting binding and update the Omarchy entry settings via
  the documented API.

### P1.5 Fix multi-monitor owner lifecycle

Implementation update (2026-09-07): watcher, call detector and pending mode state
were moved into CetraService.qml (manifest service + bar-widget). Views obtain
the common service via shell.serviceFor; canonical call settings are independent
of view lifetime. Normal bar reorder no longer needs an owner replacement.
Global plugin hot reload and disable still destroy the service; guaranteed
graceful helper/detector teardown and monitor-hotplug acceptance remain separate.

Affected code: `Cetra.qml` process creation and `cetra-watch.c` owner stdin EOF.

- Every monitor starts a watcher and call detector.
- One random monitor becomes HID owner; destroying it tears down all clients.
- Owner stdin EOF stops the daemon even when mirror clients remain active.
- Move watcher ownership and call detection into a singleton/service lifecycle,
  or at minimum retain the owner while clients exist and deduplicate polling.

### P1.6 Add shared command/pending state

Implementation verified offline (2026-09-08, supplied results): mode and settings
pending state is shared in `CetraService.qml`; ANC level, Adaptive, voice, and
proximity use desired requests separately from confirmed readback. Twelve 250 ms
scheduler ticks bound pending state, not elapsed wall time. No optimistic setter
remains for these fields; lighting retains its last-successful-command contract.
Live multi-monitor/concurrent-command acceptance remains pending. The following
bullets describe the audit, not current unchanged behavior.

Affected code: `Cetra.qml` setters and multi-monitor behavior.

- Only mode has a local pending timeout.
- Other controls update optimistically before ACK/readback.
- Pending state is per monitor, allowing contradictory concurrent writes.
- Centralize desired/pending/confirmed state in the daemon or a singleton.

### P1.7 Add response freshness/watchdog

Partial implementation (2026-09-08): settings and presence/charging have 30-second expiry and
the UI stops using old battery data to re-enable controls after presence was
observed. Battery/mode still lack response-age invalidation; periodic successful
writes do not prove receipt of new data. The first-report battery fallback also
remains. A general response watchdog is still open.

Affected code: `cetra-watch.c` polling loop and JSON state.

- Successful writes are treated as proof that the receiver is healthy.
- Old battery/mode values can remain visible indefinitely if responses stop.
- Track monotonic timestamps and missed responses; publish `stale`/unknown and
  reconnect after bounded failures.

### P1.8 Make lighting transaction atomic from the state model

Implemented state-model mitigation (2026-09-08), acceptance pending: effect/RGB
and the valid preference flag update only after every write succeeds. Reconnect
write failure resets the receiver rather than replacing the previous preference.
This is not hardware transaction atomicity or readback: partial writes may
already have changed a zone. Official Off/duplicate-commit validation remains
separate from mocked failure coverage.

Affected code: `cetra-watch.c` `set_lighting()` callers.

- Return values are ignored and UI state is updated after partial failures.
- Reverify exact Off sequence and whether duplicate commits are necessary using
  captured official traffic.
- Update desired state only after the full transaction succeeds.

## P2: IPC, Filesystem, and Security Hardening

### P2.1 Preserve IPC framing after command failure

Implemented in current source (2026-09-08), acceptance pending:
`consume_commands()` continues after failure with the transport disabled for
remaining commands in that block; owner client processing no longer skips
later clients via short-circuit evaluation. See `cetra-watch.c:741-758,898-906`
and `tests/lighting-safety/`. The following bullets describe the audit target.

- Reset command buffer framing before executing the parsed line.
- Continue parsing remaining commands in the same input block after an error.
- Always process each client's input; combine errors without short-circuiting.

### P2.2 Handle partial UNIX socket writes

Partial implementation verified offline (2026-09-08, supplied results): owner
`send_line()` advances through partial sends, uses nonblocking/no-SIGPIPE flags,
and bounds `EINTR` retries. Backpressure (`EAGAIN`) or failure disconnects that
client; there is no queued lossless delivery. Mirror stdin-to-socket forwarding
and broader stdout/client backpressure remain open. Do not close P2.2 globally.

- `SOCK_STREAM` does not guarantee one `send()` transmits the whole JSON line.
- Add bounded per-client output buffers with `POLLOUT`, or a robust write-all
  strategy for blocking paths.
- Handle `EINTR` and `EAGAIN` explicitly.

### P2.3 Strictly validate IPC values

Implementation verified offline (2026-09-08, supplied results): non-lighting
commands now enforce exact domains and reject trailing arguments; ANC level uses
checked conversion and invalid booleans cannot become false. Lighting retains
strict RGB validation. LF and CRLF terminate frames; embedded NUL invalidates a
whole frame and embedded CR is rejected. Framing continues after malformed input
or transport failure. Supplied IPC coverage is 1464 cases; parent aggregate/live
acceptance remains pending. The following bullets are the audit requirements.

- Replace `atoi()` with checked `strtol()`.
- Reject trailing garbage, overflow, invalid booleans, and RGB outside `0..255`.
- Invalid input must not silently become `off` or wrap modulo 256.

### P2.4 Harden runtime path fallback

Still open: unique owner-only `mkostemp` cache replacement and checked runtime
path lengths do not validate the socket/lock parent or remove fixed `/tmp` names.
Log destination checks are separate and do not make this runtime fallback safe.

- Fixed names directly under `/tmp` are unsafe on multi-user systems.
- Require a valid `XDG_RUNTIME_DIR`, or create a verified owner-only
  `/tmp/rog-cetra-control-$UID/` directory with `0700` permissions.
- Check every `snprintf()` result for truncation.

### P2.5 Harden telemetry permissions and I/O

Partial implementation verified offline (2026-09-08, supplied results): logging
is enabled only after owner election, with `umask(0077)` set at main entry.
No-follow opens, owner/type/single-link checks, `fchmod(0600)`, and trusted log
directory checks protect writes. An accepted older `0644` log becomes `0600` on
its first owner write; existing untouched `.old` backups are not retroactively
repaired. Directory checks cover the opened leaf and parents used in creation,
not every ancestor of an existing path. Unsafe log destinations are rejected;
this does not harden the runtime socket/lock `/tmp` fallback. Cache replacement
uses unique `mkostemp` files and checked write/close/rename instead of a predictable
temporary file. Parent live permissions/rotation verification is pending.

The permission requirements below are implemented offline. Synchronous per-event
I/O, whole-path trust, and logging policy/opt-out remain open.

- Set `umask(0077)` before the first log write.
- Open the log with `O_APPEND|O_CREAT|O_CLOEXEC|O_NOFOLLOW`, mode `0600`, then
  `fdopen()`.
- Verify owner and regular-file type using `fstat()`.
- Do not perform synchronous `stat/open/close/rotate` filesystem work for every
  HID packet in the hot loop; maintain a controlled fd or buffered logger.
- Document telemetry content and provide an opt-out/clear procedure.

### P2.6 Distinguish receiver absence from open errors

- `open_receiver()` currently collapses not-found, permission-denied, busy, and
  open failure into `NULL`.
- Publish actionable status codes that the existing UI can display.

### P2.7 Strengthen packet validation

- Require gesture report length sufficient for every accessed byte.
- Treat battery values `101..254` as protocol errors, not as normal case state.
- Validate boolean readbacks strictly as `0` or `1`.
- Validate or document battery mask semantics.
- Saturate missing counters and event counters.

## P3: QML and UX Correctness

### P3.1 Remove duplicate keyboard handling

Implementation verified offline (2026-09-08, supplied results): the standard
panel dispatcher owns shortcuts through one text path. Focused controls forward
to it and consume residual events to avoid duplicate bubbling. Local RGB arrows
remain edits, not duplicate shortcuts. The 21 supplied keyboard fixtures do not
prove rendered Qt delivery; one command per keypress, modifiers and alternate
layouts still need live acceptance. The following bullets describe the audit.

- The same keys are processed by both `PanelKeyCatcher.onTextKey` and an extra
  `Keys.onPressed` block.
- Keep one path to ensure one hardware command per keypress.
- Do not advertise `M Mute`: it changes call-gesture mode, not microphone mute.

### P3.2 Implement full keyboard accessibility

Implementation verified offline (2026-09-08, supplied results): visible/enabled
controls are collected panel-wide; Tab/Shift+Tab traverse them before switching
panels at the boundary, and focus scrolls the target into view. Activation,
collapse/hide handling, RGB arrows and explicit Apply share that focus model.
Unknown toggles explicitly request On; manual ANC selection also disables On or
Unknown Adaptive. Full rendered focus order, accessibility and scrolling remain
pending in Omarchy. The original missing-focus-model bullets below are historical.

- Arrow/Enter events are currently consumed without a cursor model.
- Tab switches panels rather than traversing controls.
- Add a proper focus/cursor model or allow native focus traversal for every
  control.

### P3.3 Use one click owner per toggle

Implemented offline (2026-09-08): `SettingToggle` uses a single row click owner
and a non-interactive inner switch; accessibility activation uses the same
action. Rendered label/switch click acceptance is still pending.

- `ToggleSwitch` and an overlaying full-row `MouseArea` both define toggle
  handlers.
- Use standard `Toggle`, or set the internal switch non-interactive and let only
  the row own clicks.

### P3.4 Add scrolling for constrained displays

Implemented in source (2026-09-08): fitted content height and a clipped vertical
`Flickable` are present (`Cetra.qml:302-303,373-387`). Small-screen/high-scale and
keyboard scrolling acceptance are pending; the original plain-Column problem
below is historical.

- Panel content is a plain `Column` under a fitted height.
- Wrap it in a clipped `Flickable`/`ScrollView` so all controls remain reachable
  on small screens and high UI scales.

### P3.5 Close popup when the widget becomes hidden

Implemented during integration (2026-09-08): hiding the widget closes its popup.
Live hide-while-open acceptance remains pending; the following is the audit case.

- If receiver visibility hides the widget while its panel is open, the popup
  surface can remain open with a stale/hidden anchor.
- Bind panel open to widget visibility or explicitly close on hide.

### P3.6 Use a real Omarchy accent token

Current source (2026-09-08) falls back to `Color.accent` when `bar.accent` is
absent. The palette's theme selection uses that value only on explicit apply;
this is not automatic hardware theme synchronization.

- Current Omarchy Bar has no `bar.accent` property.
- Use `Color.accent` directly for theme-aware Aura RGB.

### P3.7 Fix unavailable/in-case wording

Partial mitigation (2026-09-05): QML battery fields/tooltips now show `No data`
for unknown values, including the case; disconnected status refers to missing
telemetry rather than asserting physical placement. Daemon lifecycle logging
still needs the same correction.

Current source update (2026-09-08): lifecycle log messages now say earbud
telemetry unavailable/available (`cetra-watch.c:389,975`). Presence/charging
labels remain separate from last reported percentages; none proves case placement.

- A missing earbud can also be powered off, out of range, or unsynchronized.
- Use `Unavailable or in case` for earbuds.
- Do not show `In case` for the charging case itself; use `Unavailable` or `—`.

### P3.8 Remove dead QML state

Current source update (2026-09-08): `tapSeq` is no longer a view property and
`modeOptions.shortcut` is used in button tooltips. Duplicate receiver assignment
remains in `CetraService.qml:74,82`. Treat the original list below as audit
history, not three unchanged defects.

- `tapSeq` is unused.
- `modeOptions.shortcut` is unused in current rendering and input generation.
- `receiver` is assigned twice in the same status update.

### P3.9 Validate extensible localization without claiming translations

Implemented in current source (2026-09-08): `I18n.qml`, English catalog/index,
widget `tr()` calls, `locale: system`, exact/base/en fallback, named placeholders,
and RTL panel mirroring. Add languages through validated catalog registrations,
not hardware voice-prompt values. No translations were requested or added;
manifest settings labels remain English. Keep real Qt asynchronous loading,
long-text layout, RTL focus order, and failure fallback in acceptance coverage.

## P4: Test Coverage Backlog

Current integration update (2026-09-08): the translated Unknown guard and RGB/i18n
suites are wired into `tests/run.sh`, together with settings, IPC and log safety.
Supplied green results include 10607 settings parser events / 36 owner scenarios,
1464 IPC cases, 181 lighting-safety scenarios, and 21 keyboard fixtures. These
were not run in this documentation pass; current combined parent verification
and rendered/live acceptance remain pending. Earlier results in the review stay
scoped to the integration that produced them.

Test portability debt: the harness currently requires an existing `/tmp/opencode`
parent for temporary artifacts. No OpenCode installation or configuration-folder
requirement should be added. A portable temporary-root change is deferred; no
tests or setup are changed by this documentation pass.

Required test areas:

1. Microphone state cannot be claimed absolute from edge events alone.
2. No automatic lighting write on first battery response.
3. Golden Aura sequences for every effect and both zones, including failure at
   every transaction step.
4. Mocked verified query schedule for `07`, `25`, `2b`, `2c`, `28`, and `26`.
5. Missing, delayed, duplicated, reordered, and invalid HID responses.
6. Packet lengths `0..64` and invalid value domains.
7. Socket partial writes, slow clients, `EINTR`, `EAGAIN`, and broken clients.
8. Multiple commands per `recv()` and HID failure on the first line.
9. Multi-client owner stdin EOF while mirrors remain active.
10. Watcher restart during active call context.
11. `pactl`/`jq` error and timeout after active context.
12. Two-monitor concurrent commands and shared pending state.
13. Settings persistence through shell reload.
14. Pointer click on label vs switch without double toggles.
15. Keyboard events for English/Russian layouts and modifier combinations.
16. Small-screen/high-scale panel scrolling and accessibility tree.
17. Filesystem failures, symlink attacks, long paths, and `ENOSPC`.
18. Distinct not-found, permission-denied, and open-failure statuses.
19. `cetra-status` should test shared production packet builders rather than a
    private copy.
20. Presence `01` startup/10-second schedule, unanswered-query expiry, reconnect
    failures, and unchanged C `connected` semantics; owner mock coverage exists.
21. Manual RGB validation, persisted selection without autosend, explicit apply
    for colored effects, and theme changes; offline function coverage exists.
22. Locale catalog validation, exact/base/English fallback, placeholders, stale
    asynchronous completions, and RTL metadata; offline function coverage exists.

Existing suites implement parts of this list; their presence does not establish
fresh passing results, full Qt event delivery, or physical device behavior.

## Verified Audit Evidence

At audited commit `094a649`:

- `./tests/run.sh`: passed.
- `omarchy plugin validate .`: passed.
- `git diff --check`: passed.
- GCC strict build with `-Wpedantic -Wconversion -Wshadow -Wformat=2
  -fanalyzer -Werror`: passed.
- AddressSanitizer + UndefinedBehaviorSanitizer selftests: passed.
- Manifest JSON parse: passed.
- Symlink scan: no symlinks.
- `qmllint`: unavailable; not run.
- `shellcheck`: unavailable; not run.

These passes establish basic structural and memory-safety confidence for tested
paths only. They do not override the behavioral blockers in this backlog.

## Definition of Done for the Next Release

A new release candidate is ready only when:

1. All P0 items are resolved.
2. Public docs match production behavior and verified evidence.
3. State fields distinguish desired, confirmed, unknown, and stale where needed.
4. No hardware settings are changed on startup without explicit intent.
5. Runtime lifecycle is stable across shell restart and multiple monitors.
6. New regression tests cover every fixed P0/P1 bug.
7. The exact candidate commit passes mandatory verification.
8. The manifest version is bumped beyond `1.5.0` and the new GitHub tag points
   to that exact reviewed commit.
