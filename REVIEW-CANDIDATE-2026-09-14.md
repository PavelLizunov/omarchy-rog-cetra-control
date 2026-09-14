# Candidate self-review — 2026-09-14

Status: release preparation with user-accepted limitations; omitted live tests
remain open. This is direct self-review,
not independent approval. Base d5a687b plus the current local changes; source
hashes for executed native suites are emitted by their runners.

Reference: Observatory 9ddfc678e5cbcfb882d27997aaf8afcfcd872c3b,
particularly PAT-SAFE-ARGV, PAT-REACTIVE-SERVICE, PAT-BOUNDED-INPUT and
PAT-EVIDENCE-GRADE. Research examples are not an automatic safety certificate.

## Six dimensions

| Dimension | Protective evidence | Remaining boundary |
| --- | --- | --- |
| Manifest / delivery | Namespaced bar-widget + service; version 1.7.0; source-built ignored helpers; isolated setup tests | Exact publication commit and author's submission-rights confirmation pending; preview is explicitly historical |
| Processes / privileges | One elected HID owner; audio/settings helpers have no HID; argv arrays; fixed settings path; no software media/mute injection | Plugin is unsandboxed; kernel/filesystem stalls can defeat userspace stop deadlines |
| Timers / cadence | Native topology observation replaces recurring CLI; one-shot settle/retry; monotonic peak timer | HID loop still checks at 50 ms to preserve event latency; no physical energy claim |
| Resilience / data | Fixed native frames/queues; settings 1 MiB before output; tri-state capture; null/zero distinction; domain/freshness checks | Bundled locale FileViews trust source-controlled files; no guarantee against malicious rewriting of the plugin itself |
| Memory lifecycle | Fixed client count/queues; reader serialization/generations; locale readers destroyed on replacement; helper cleanup tests | Mixed 30-minute RSS/FD trial is not heap proof or every lifecycle combination |
| Theme / displays | Host tokens, bounded panel, explicit view dependencies, warning contrast fallback | Multimonitor hotplug, full screen-reader/RTL and rendered cross-theme acceptance pending |

## Differential security review

Reviewed data flows:

- Host configuration -> watch notification -> fixed argv cetra-status --read-settings
  -> regular owner file -> <=1 MiB buffer -> exit-checked QML parsing. No shell
  interpolation, caller-selected read path or write operation in this helper mode.
- PipeWire metadata -> bounded graph traversal -> separate endpoint/communication
  observations -> service call intent or admitted peak process. Metadata names do
  not become shell code. Privileged local metadata manipulation is not prevented.
- Peak floats -> finite/range check -> bounded JSON -> nullable visual level.
  No PCM persistence, HID, application-route changes or mute inference.
- Socket commands -> fixed frame buffer -> validated enums/integers -> known HID
  builders. Same-owner runtime directory and lock checks reduce cross-user path
  risk; descriptor-relative same-user race resistance remains incomplete.
- Setup -> explicit unlocked status -> isolated build/selftests -> staged rename.
  Compile failure preserves current binaries. Lifecycle signals target helpers
  from this checkout, including a replaced executable's deleted-path identity.

No new demonstrated critical exploit was found in these flows. This is not a
claim that every repository asset or dependency has independent security approval.
Local package installation is disclosed and remains user-authorized setup work.

## Anti-slop / prose review

Scope: current README, HANDBOOK ownership/behavior sections, RELEASE, BACKLOG,
CONTRIBUTING, current research clarifications and English/Russian UI strings.
Historical reports remain dated evidence, not current instructions.

- PASS: removed assertion that an observed vendor tap guarantees media Play/Pause.
- PASS: documented the reproduced background-capture limitation without pretending
  to fix another project's service or adding a software gesture substitute.
- PASS: separated published 1.6.0 from local work, removed stale release claims
  about the obsolete pactl/jq runtime path, linked current acceptance evidence.
- PASS: Russian charging copy now says "По отчету"; signal label avoids unexplained
  English "mute" while retaining the distinction from hardware state.
- PASS: source-token warning contrast defect has a local fallback, not a theme edit.
- NOT VERIFIED: every catalog's fluent-human meaning, full rendered contrast,
  all accessibility/RTL states and a new public preview. Key/placeholder coverage
  and selected Qt label geometries are separate automated evidence.

UI purpose: compact native Omarchy hardware control, inherited typography/spacing,
one narrow signal bar, no synthetic animation or invented telemetry. Existing
desktop conventions take precedence over generic web/mobile design recipes.

## Review handoff

An external reviewer should inspect the full task-owned tracked/untracked diff,
PROTOCOL-COMPLIANCE.md, acceptance ledger and all new native/test paths. Re-run
the aggregate suite and applicable sanitizers against the exact candidate.
Focus on backpressure/HUP, native read cancellation, filesystem trust boundaries,
PipeWire topology ambiguity and reload ordering. Do not claim independence from
this self-review or authorize publication through a review artifact.

Remaining decisions are recorded in BACKLOG.md and RELEASE.md. User prohibited
changes to other projects; no such changes are part of the candidate.
