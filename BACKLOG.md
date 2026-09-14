# Active backlog

Updated 2026-09-14. This file owns release status; RESEARCH.md owns protocol
evidence. The detailed pre-modularization audit is preserved in
[docs/archive/BACKLOG-2026-09-14-before-modules.md](docs/archive/BACKLOG-2026-09-14-before-modules.md).

## Release status

Version 1.6.0 is published as a GitHub pre-release at commit d5a687b, not a stable
release. Do not overwrite existing tags. Automated checks passed on that snapshot;
final modular connected-device acceptance and Marketplace submission remain
pending. See RELEASE.md for the evidence and release URL.

The user accepted the documented compatibility/test limitations and requested
release preparation. Version 1.7.0 is the candidate. Accepted limitations below
remain visible; this decision is not a claim that omitted tests passed.

## P0 — release acceptance

| ID | Implementation / evidence | Remaining acceptance |
| --- | --- | --- |
| P0.1 False absolute microphone state | Removed inferred mute and manual resync. JSON stays unknown in offline and live trials | Implementation closed; unavailable native readback is an accepted product limitation |
| P0.2 Implicit lighting writes | Starts unknown; explicit session preference only. 181 mocked transaction cases and live effects/restart checks pass | Fresh colored case/USB replay retest deferred and accepted for this candidate; exact official Off/duplicate sequence remains a documented research limit |
| P0.3 Fabricated settings | Nullable settings, 30 s freshness and presence veto. Parser/owner tests and live controls/case/USB pass | Implementation closed; stopped-report expiry is verified offline, not by deliberately disrupting hardware |
| P0.4 Call restart reconciliation | Latest-owner Discord restart recovered context; user confirmed Off/On prompts; ending call stopped the meter | Passed for the recorded Discord/EasyEffects trial; requested context is not tap/mute readback |
| P0.5 Truthful publication | Version 1.7.0, release notes and accepted limitations prepared; historical preview labeled | Delivery gate: validated source archive, exact publication commit and separately authorized publication; author confirms asset submission rights |

P0 runtime fixes and accepted limits are distinct from the publication gate.
Do not claim the candidate is already published or independently verified.

## P1 — runtime and host integration

- **Media tap / continuous capture:** marked A/B/A trial reproduced lost native
  Play/Pause with Voxtype keepalive capturing via EasyEffects, recovery with no
  capture, and recurrence after restoring keepalive. Plugin context was false
  and meter absent. See RESEARCH.md. Keepalive was restored. User explicitly
  excluded changes to other projects/services. Gesture logs now report the
  observation without promising media-key delivery; README documents the limit.

- **General freshness:** presence/charging/settings expire; new battery_fresh and
  mode_fresh flags expire after 30 seconds and veto stale UI. Exact hardware expiry
  acceptance remains pending; legacy raw fields retain diagnostic history.
- **Teardown:** one live disable/re-enable test ended the owner normally and
  started one replacement. This is not proof for a hung detector or all reload races.
- **Host preference loss:** installed Omarchy removed inline preferences during
  disable/re-enable. The test restored known preferences. Document backup before
  disabling; a plugin-local disk reader cannot preserve a host-deleted entry.
- **Hot reload:** stale QML was observed after logged reloads. Verify actual
  visible changes; obtain/retain explicit authorization for a shell restart.
- **Call semantics:** generic untagged browsers no longer match the name fallback.
  Known communication apps still use a heuristic when role metadata is absent.
  Manual request UI/M/Ь were removed; legacy alwaysCallContext is ignored.
- **Multimonitor:** shared-state offline coverage exists; monitor removal and
  concurrent live controls on multiple displays are not fully verified.
  User has no second monitor and explicitly requested leaving this open.
- **Suspend/resume:** user deferred the live trial as inconvenient; keep OPEN.

## P2 — transport, filesystem and resource limits

- Shared `/tmp` fallback removed; private runtime root and lock validation added.
  Descriptor-relative protection against concurrent same-user path changes remains open.
- Owner stdout and mirror buffers are bounded; owner blocked/partial output passes
  sanitizer tests. Real private mirror backpressure/ordered EOF drain passes;
  stalled EOF drain has a two-second deadline.
- Log/cache file hardening includes ancestor checks. `CETRA_DIAGNOSTICS=0` disables
  new log writes. Logging/cache remains synchronous; pathological filesystem stalls
  are an explicit remaining constraint, not a claim of bounded I/O latency.
- HID open failure still collapses absence, permissions and busy failures.
- Missing-battery counters saturate at two; UI rejects invalid battery domains.
  Hardware mask interpretation remains a research constraint.
- Host config is read by the existing cetra-status helper with a 1 MiB pre-output
  limit and three-second deadline. FileView only watches. Reads are serialized
  and stale generations rejected. User confirmed language/option persistence
  after the new reader was installed and the shell restarted.

## P3 — presentation and tests

- Unreleased opt-in microphone meter uses a libpulse peak helper. Exact
  source/link gates and self-exclusion pass offline tests. Live Discord capture
  produced one meter and a nonzero level; opting out removed its stream without
  stopping Discord. Marked call-exit and restart trials passed. This does not close
  native mute research.
- EasyEffects rerouting is prevented on the tested PipeWire 1.6.8/WirePlumber
  host by node.dont-move plus the Pulse flags. The helper checks the actual
  source and exits on mismatch. Live peaks, unchanged Discord routes and EOF /
  opt-out teardown passed; no manual EasyEffects exclusion was added. Physical
  USB recovery passed; ending the last admitted endpoint removed the meter even
  though the existing keepalive remained. See ACCEPTANCE-2026-09-14.md.
- Endpoint admission now excludes processing-only/keepalive routes. Live owned
  Production capture started one meter without call context; stopping it removed
  the meter despite the pre-existing keepalive. Real Discord call and restart
  acceptance passed with user-confirmed native Off/On prompts and level movement.
- Battery observation 2026-09-14: left battery became ff while fresh presence
  remained 11. UI separates availability from unknown percentage; reporting loss
  remains unresolved and no stale value is fabricated.

- Latest-owner USB/case transitions, all ANC levels/modes, voice settings and
  physical lighting effects were exercised. Evidence is in ACCEPTANCE-2026-09-14.md.
- Basic live keyboard traversal/activation/Escape and RGB editing passed by user
  confirmation. Full locale/RTL, screen-reader and rendered light/dark contrast
  remain incomplete. Warning contrast now has a theme-token fallback; never treat
  placeholder equality as fluent-human review.
- Test artifacts require `/tmp/opencode`; portable temporary roots are deferred.
- Offline QML tests inspect production functions, but do not prove Qt signal
  ordering or component ownership. Keep live checks after extraction/refactoring.

## Research, not release features

- No confirmed absolute native mute readback or host-native mute toggle.
- Proximity setting/readback is known; USB auto-pause delivery is unconfirmed.
  The UI/P/З shortcut remain absent; removal did not write the hardware setting.
- Runtime Off uses Static with black RGB and two commits; exact official sequence
  and need for duplicate commits require an annotated official capture.
- Ten EQ values have only eight documented frequencies. Complete the map before UI.
- Absence of a HAL/UI capability does not prove absence in every firmware path.

## Definition of done

1. Close P0 acceptance with evidence for the exact candidate.
2. Keep unknown, desired/requested and confirmed values distinct.
3. Preserve single HID ownership, verified opcodes and the screen-lock guard.
4. Run `./tests/run.sh`, `omarchy plugin validate .`, `git diff --check`.
5. Verify actual panel delivery and authorized lifecycle on the installed host.
6. Commit/push/tag/submit only with explicit publication authorization.
