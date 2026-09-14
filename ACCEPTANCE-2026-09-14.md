# Acceptance evidence — 2026-09-14

Scope: local unreleased work after d5a687b. No publication approval. Observed host:
Omarchy 4.0.3-1, Quickshell 0.3.1-1, WirePlumber 0.5.17-1, PipeWire 1.6.8.
Expanded owner source tested by sanitizers:
`8e539e9621143b5746ef2f106edc84c4762205454f7a0a69ecad2689178207be`.
Wall-clock times below are the machine log timestamps; actions were confirmed
by the user in the same session. PCM was neither saved nor decoded.

## Audio and calls

- Processing-only: existing EasyEffects + pw-record /dev/null did not launch
  cetra-peak after source delivery. An owned Production recording to discarded
  stdout started one meter without call context; ending it removed the meter.
- First marked Discord trial: call_context true, Discord on EasyEffects source,
  meter on physical Cetra. User confirmed Off/On prompts and responding scale.
  Two reports: 12:21:13.489 and 12:21:21.059. After leaving the call, context false,
  no meter, existing keepalive still running. This trial preceded final owner install.
- Latest-owner active-call restart: shell restart explicitly authorized. New owner
  PID 3347412, one peak client. At 13:33:10.824 call off was sent during initial
  observation, followed by call on at 13:33:11.244. No startup RGB command occurred.
  User confirmed prompts/scale and no spontaneous lighting. Reports at
  13:33:46.544 and 13:33:49.555; exit call off at 13:34:05.685, meter disappeared.
- These tests validate requested context and observed physical prompts in the
  stated trials. They do not establish an absolute mute readback or all-app support.

## USB and case transitions

- User removed USB receiver: receiver/connected false, nullable values cleared,
  freshness false, no peak client. Owner PID 3243180 stayed alive.
- User restored receiver: same owner recovered, fresh battery/mode/settings
  returned, context false and no idle meter. Lighting remained unknown.
- Both earbuds in closed case: presence 00, settings unknown, no meter. Raw daemon
  connected remained battery-derived; UI uses the presence veto, not that raw flag.
- Right only: presence 10, left battery null, right available.
- Left only: presence 01, right battery null, left available.
- Charging reports sometimes outlasted extraction; TTL later cleared them.
  Reported charging does not measure electrical charging current.

## Controls

- User exercised Off/ANC/Ambient/ANC, Low/Mid/High, Adaptive On/Off.
  Log confirms mode transitions and readbacks. Mid/High responses arrived about
  8.7 seconds after request, inside the nominal 12-second pending window.
- Final state: ANC, level 3, Adaptive false.
- Voice Chinese/Sound/English: commands/readbacks at 13:31:05, :18 and :23.
- User physically confirmed Static/Breathing/Strobing/Cycle/Off lighting.
  Commands at 13:31:32–:50; explicit colored effects used RGB 121/129/134.
  Last selected lighting was Off. Lighting JSON is last sent, never readback.

## Resource observation

`python3 -B tests/resource-observe.py --seconds 1800`: 1800.26 seconds, no read errors.
User confirmed changing capture/earbud state during the window. This is a mixed
workload trial, not an uninterrupted 30-minute capture or controlled A/B benchmark.

| Process | Samples | RSS KiB first → last (min..max) | FD | Threads | CPU % of one core |
| --- | ---: | --- | --- | --- | ---: |
| cetra-watch 3243180 | 178 | 2532 → 2480 (2480..2532) | 8 | 1 | 0.081 |
| cetra-peak 3244195 | 38 | 5212 → 4556 (4556..5212) | 9 | 1 | 0.251 |
| cetra-peak 3269800 | 117 | 5052 → 5052 | 9 | 1 | 0.218 |
| shared quickshell 3242888 | 178 | 479640 → 365496 (356872..479640) | 111..128 | 40..50 | 10.061 |

No sustained RSS/FD growth observed. Sampling does not prove absence of short-lived
overlap or slow leaks. Shell CPU/RSS includes every plugin and is not attributable
to Cetra. Physical energy remains unmeasured. No Valgrind/heaptrack was installed.

## Automated checks

- Aggregate tests, manifest validation and git diff --check passed before install.
- ASan/UBSan with leak detection: microphone-state (101 events), device-reports
  (3916 events, max JSON line 487 bytes), IPC and log/cache suites passed.
- Peak helper ASan/UBSan: invalid sources, unavailable/silent protocol peer,
  three-second startup deadline, stdin EOF and SIGTERM passed without audio capture.
- Mirror: real private socket/pipe test with held election lock, 400 KB status and
  180 KB command traffic, bounded queues, ordered transport and EOF drain passed.
- Setup: isolated real builds in spaced path, second-run inode/mtime preservation,
  compile-failure preservation, explicit unlocked-only guard passed.
- QML: all 19 files pass scoped semantic policy; 55 host dynamic members verified
  from source declarations, one unused QProcess enum exception, lexical-access
  style advice retained as info. Not a claim of zero raw qmllint diagnostics.

## Still requiring acceptance or a scope decision

- New media-gesture finding: A/B/A confirmed native Play/Pause fails with the
  Voxtype/EasyEffects keepalive capture, works with that service stopped (actual
  Consumer 0x08 reports), and fails again after restoration. No meter/call request
  was required for failure. Service is restored; persistent policy is undecided.

- Explicit colored lighting replay through case/USB and auto-theme after reconnect.
- Suspend/resume, multimonitor hotplug, screen-reader, exhaustive keyboard/locale
  layout and light/dark contrast trials.
- Same-user path replacement policy and synchronous logging/cache latency remain
  explicit constraints. Settings byte bounding and diagnostic opt-out were added
  after the resource trial; see the follow-up below.
- Independent review, final clean versioned candidate, exact publication commit.
- User explicitly declined suspend/resume for now and has no second monitor.
  Both live scenarios remain OPEN by agreement, not passed or waived.

## Follow-up implementation and user acceptance

- Existing cetra-status now reads the fixed host settings path with a 1 MiB limit
  before output, a three-second deadline and regular-file/owner checks. FileView
  only watches changes. Exact/over-size, missing/FIFO/directory tests passed.
- Setup first refused an unconfirmed lock state; after an explicit unlocked status
  it installed the new helpers and performed the single authorized shell restart.
  User confirmed RU/EN/RU, meter off/on and close/reopen preference persistence.
- Peak lifecycle test covers parent death while stdin stays open, in addition to
  EOF/SIGTERM and silent-server startup timeout. Pulse publication now uses
  monotonic rttime events. This invalidates performance attribution to the older
  exact helper but does not erase the earlier mixed-workload observation.
- CETRA_DIAGNOSTICS=0 suppresses new log writes; isolated owner test verifies it.
- User confirmed Tab/Shift+Tab, arrows, Enter/Space, Escape and RGB edit behavior.
- Colored case/USB replay was not repeated at user's request; historical success
  was recalled but not upgraded to a fresh exact-snapshot trial.
- Current-theme token calculation: normal text 11.56:1, 85% secondary text 8.58:1,
  selected text 7.74:1, accent focus 4.70:1, original urgent text 2.78:1. Opaque
  surfaces only; this is not a screenshot/rendered-all-states contrast certificate.
  A production contrast guard now falls back to theme foreground for warning
  text under 4.5:1; dark/light regression cases pass.
