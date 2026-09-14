# Completion plan

Approved by the user in chat, 2026-09-14. This is an execution ledger, not release
approval. Base: d5a687b5ff2a4cd9482a83ea2acb0773d6d8d7c9 plus existing local work.
Observatory reference: 9ddfc678e5cbcfb882d27997aaf8afcfcd872c3b.

## Intent and invariants

Complete the agreed runtime, lifecycle, resource, documentation and publication
preparation work. Preserve one HID owner, verified commands, nullable telemetry,
Unknown native mute, explicit lighting authorization and the lockscreen guard.
Audio metering must not authorize calls or sustain its own capture admission.
No existing application routing or EasyEffects settings may be changed.

## Interface and data contract

- External capture and communication capture are separate tri-state observations.
- Both require a verified active path from the physical Cetra input to an external
  endpoint; processing/monitor/keepalive nodes alone are not endpoints.
- Ambiguous routes, incomplete objects and unavailable servers fail closed.
- One service owns topology observation and one optional peak helper.
- Numeric level is visual signal only; null is unavailable, never native mute.
- Resource ownership, budgets, stop paths and stale-result handling are explicit.

## Acceptance ledger

- [x] Record approved scope, base and Observatory reference.
- [x] Reactive audio topology, endpoint exclusion and call/meter separation.
- [x] Event-driven peak helper, bounded retries and honest exit codes.
- [ ] Process cancellation, EOF, parent death, reload and source replacement.
- [x] Nonblocking bounded owner/mirror transport and fair client admission.
- [ ] Private runtime paths, checked filesystem objects and safe setup/update.
- [ ] Remaining BACKLOG state/freshness/logging/preference defects reconciled.
- [x] Every production HID write mapped to RESEARCH evidence (PROTOCOL-COMPLIANCE.md).
- [x] Scoped semantic QML lint with actual host imports and explicit tooling exceptions.
- [x] Regression, sanitizer, setup and 30-minute mixed-workload resource trial.
- [ ] User-assisted USB/case/gesture/call/suspend hardware acceptance.
- [ ] Multimonitor, keyboard, long-text, RTL and contrast acceptance.
- [ ] README/HANDBOOK/MODULES/BACKLOG/RELEASE/RESEARCH/CHANGELOG consistency.
- [ ] Locale meaning/placeholders and anti-slop UI/prose review.
- [ ] Source/license/private-data review and exact candidate identity.
- [ ] Independent review: external reviewer required; Astra cannot delegate.
- [ ] New version, clean source candidate and Marketplace submission material.

Required gates: ./tests/run.sh; omarchy plugin validate .; git diff --check.
Runtime gates: one HID owner, valid status JSON, continuing diagnostics, actual
consumer delivery. Test results apply only to their identified snapshot.

Hardware actions and interrupting lifecycle trials require marked user windows.
Commit, push, release and Marketplace submission require separate authorization.
Research-only absent capabilities cannot be marked implemented or verified.

## Iteration evidence (not final acceptance)

- Added AudioTopology using existing Quickshell PipeWire objects and active links.
  Offline checks cover direct/processed/foreign/mixed sources, cycles, budget,
  monitor exclusion, keepalive exclusion and explicit non-call roles.
- Authorized setup and one shell restart delivered the new source. With only
  EasyEffects -> pw-record /dev/null remaining, no cetra-peak existed. An owned
  six-second Production capture to discarded stdout started one cetra-peak on the
  physical Cetra input; call_context remained false. After stopping that capture,
  cetra-peak disappeared while the pre-existing keepalive continued.
- Replaced peak's 10 ms polling with Pulse mainloop I/O and publication events.
  Startup timeout is now an error. Meter retry is one-shot.
- Owner stdout has bounded partial/latest-frame buffers; mirror uses bounded
  bidirectional buffers. IPC sanitizer checks exercise blocked/partial stdout
  and coalescing; broader lifecycle and mirror tests remain pending.
- Removed shared /tmp fallback; runtime root and lock object are checked.
- Setup fails closed for empty/malformed/unknown/locked responses and stages
  replacement in bin/ before rename. Dedicated guard fixtures pass.
- QML lint now resolves qs imports through isolated ordinary-file copies. It
  exposes dynamic QObject typing and unqualified-access warnings; strict full
  lint remains OPEN. AudioTopology/CallDetector/MicrophoneMeter pass -W 0.
- Resumed acceptance: real Discord call through EasyEffects produced call_context
  true and one physical-source peak stream. User confirmed native Off/On prompts
  and responding level. Owner logged taps at 12:21:13.489 and 12:21:21.059, seq 1/2.
  After the user left the call, call_context became false and the meter exited;
  only the pre-existing pw-record keepalive remained. Native mute stayed Unknown.
  This used the installed topology observer but an older owner binary without
  battery_fresh/mode_fresh; it is not full final-binary acceptance.
- Further latest-owner USB/case/controls/call-restart trials and resource evidence
  are recorded in ACCEPTANCE-2026-09-14.md. Remaining limitations are explicit.
- User boundary: do not modify other projects or services. Voxtype keepalive was
  restored after the separately authorized diagnostic A/B/A; no permanent change
  is authorized. Cetra documents the continuous-capture media-gesture limit.
- Settings acquisition now uses existing cetra-status in a fixed-path, 1 MiB /
  three-second mode; FileView is watch-only. Dedicated exact/oversize/FIFO tests
  pass. After setup/restart user confirmed RU/EN/RU and meter opt-out/in persisted.
- User confirmed basic live keyboard traversal, activation, Escape and RGB editing.
  Latest colored replay retest was declined as inconvenient; retain historical
  evidence without marking that exact-candidate trial complete.
- Source-token contrast check identified 2.78:1 low-battery color on Solitude.
  Cetra now uses theme foreground when warning color is below 4.5:1. Offline
  dark/light regression passes; full rendered cross-theme acceptance remains open.
- Latest native suites: expanded owner SHA-256
  567da34b148a90eeb1fff4c688caf476c17249f5e021595c457955f919c38689;
  microphone-state, device-reports, IPC and log/cache ASan/UBSan passed.
  Settings and monotonic peak helper sanitizer checks passed, including parent
  death, silent-server timeout, exact/oversize reads and rejected special files.
  These results supersede previous source identities for the same checks only.
- User explicitly left suspend/resume and two-monitor acceptance OPEN: testing
  sleep is inconvenient and a second monitor is unavailable. Do not substitute
  offline coverage or the mixed-workload trial for these live scenarios.
- User accepted documented compatibility/test limits and requested preparation
  for release. Candidate is 1.7.0; release notes and Marketplace form draft are
  local artifacts. This approval does not authorize commit/push/tag/submission.
