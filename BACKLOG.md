# Active backlog

Updated 2026-09-14. This file owns release status; RESEARCH.md owns protocol
evidence. The detailed pre-modularization audit is preserved in
[docs/archive/BACKLOG-2026-09-14-before-modules.md](docs/archive/BACKLOG-2026-09-14-before-modules.md).

## Release status

The manifest is a 1.6.0 candidate, not a published stable release. Do not overwrite
v1.5.0. Automated checks establish their tested contracts; full release acceptance
and an exact reviewed commit remain pending. See RELEASE.md.

## P0 — release acceptance

| ID | Implementation / evidence | Remaining acceptance |
| --- | --- | --- |
| P0.1 False absolute microphone state | Removed inferred mute and manual resync. JSON stays `microphone_state: unknown`; 101-event strict and sanitizer suites pass | Preserve unknown UI on every path; no native absolute readback is known |
| P0.2 Implicit lighting writes | Starts unknown; daemon replays only a successful explicit session preference. Auto-theme opt-in is separately session-gated; 181 mocked lighting cases and QML tests pass | Full physical case/USB replay and failure acceptance remains pending; official Off/duplicate-commit sequence remains a research limit |
| P0.3 Fabricated settings | Nullable settings, domain checks, 30 s freshness, presence veto. 10607 parser events / 36 owner cases pass. Pending requests allow 48 × 250 ms ticks; late matches clear errors | Complete live checks across all exposed controls, absence and expiry on the final modular source |
| P0.4 Call restart reconciliation | Startup sends current detected intent; stopped writes do not mark intent sent. Automatic capture detector retains bounded failure handling | Active-call restart with physical gesture observation remains pending; requested context is not tap/mute readback |
| P0.5 Truthful publication | Current preview, modular documentation and protocol limits prepared | Final source/license/private-data review, exact commit, clean candidate checks and authorized publication |

P0 implementation fixes are not all release signoffs. Do not describe the whole
project as release-ready while the acceptance column remains open.

## P1 — runtime and host integration

- **General freshness:** presence/charging/settings expire; battery/mode have no
  general response-age watchdog. Battery-only fallback remains before first presence.
- **Teardown:** one live disable/re-enable test ended the owner normally and
  started one replacement. This is not proof for a hung detector or all reload races.
- **Host preference loss:** installed Omarchy removed inline preferences during
  disable/re-enable. The test restored known preferences. Document backup before
  disabling; a plugin-local disk reader cannot preserve a host-deleted entry.
- **Hot reload:** stale QML was observed after logged reloads. Verify actual
  visible changes; obtain/retain explicit authorization for a shell restart.
- **Call semantics:** generic browser recording may match the application-name
  fallback. Manual request UI/M/Ь were removed; legacy alwaysCallContext is ignored.
- **Multimonitor:** shared-state offline coverage exists; monitor removal and
  concurrent live controls on multiple displays are not fully verified.

## P2 — transport, filesystem and resource limits

- Fixed `/tmp` runtime fallback still needs an owner-private directory policy.
- Mirror forwarding and stdout backpressure are not lossless/fully bounded.
- Log/cache file hardening has offline coverage; every existing ancestor is not
  validated, logging is synchronous and has no runtime opt-out.
- HID open failure still collapses absence, permissions and busy failures.
- Missing-battery counters are not saturated; invalid battery domains and mask
  semantics need a stricter contract before changing behavior.
- FileView reads the host config without an input byte cap. Malformed reads keep
  last valid preferences; schema validation and deletion behavior need future work.

## P3 — presentation and tests

- Final modular live rendering of controls needs available earbuds. The latest
  module-load check covered the honest unavailable state with one owner.
- Full locale/RTL, modifier keys, screen-reader and light/dark contrast acceptance
  remain incomplete. Never treat placeholder equality as fluent-human review.
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
