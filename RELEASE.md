# 1.6.0 publication candidate

Status: local candidate, not a published stable release. No tag or Marketplace
submission has been created by this preparation pass. The existing v1.5.0 tag
must not be moved.

## Candidate contents

- Shared service and single receiver owner.
- Unknown hardware mute state; automatic capture-driven call requests.
- Manual call override and proximity UI removed.
- Confirmed ANC settings with a nominal 12-second readback window and late-result recovery.
- Local interface catalogs and settings persistence fixes.
- Explicit RGB application and separately enabled, session-gated theme updates.
- Theme-adaptive controls, keyboard navigation and current panel preview.

## Evidence obtained on 2026-09-14

- Aggregate tests, manifest validation and diff whitespace checks passed during
  implementation. Repeat against the final intended commit before submission.
- Live UI inspection exposed stale QML after local hot reload; authorized shell
  restart loaded the current panel. A reload log alone is not acceptance.
- Live EN/RU selection and capture-request toggle checks preceded removal of the
  manual control; subsequent inspection confirmed that control was absent.
- Live ANC readback confirmed Low and High. Other observed level changes took
  4–8 seconds, motivating the revised pending window.
- Auto-theme commands followed both a temporary theme change and restoration;
  physical lighting color remains user-observed, not protocol readback.
- One watcher owned the receiver during final implementation checks; status JSON
  was valid and telemetry continued.
- Live disable/re-enable ended owner PID 2219199 with result 0 and started one
  replacement (2226732). The host removed inline preferences; known preferences
  were restored. This tests normal exit, not hung processes or every reload race.
- Modular entry points are 207 QML lines and 277 C lines. Full C/JS suites passed
  with complete module snapshots; microphone and report suites passed ASan/UBSan.
  The rebuilt helper matched the pre-split binary SHA-256
  `feee56b2cda28c43d0828f3b328676a1c84f676ce55ea681532a88fb519b6715`.
- The modular panel loaded in the existing shell and displayed unavailable
  telemetry honestly. Connected controls still need final live acceptance when
  earbuds are available; old screenshots alone do not prove it.
- Source-only candidate setup built both helpers and passed their selftests
  twice. The second run preserved binary hashes and modification times; the
  private temporary build directory was removed. Shell lock status, device
  discovery and process enumeration were mocked: no packages were installed,
  no live service was stopped and no HID reader was opened by that test.

## P0 acceptance reconciliation

| Backlog item | Evidence | Remaining boundary |
| --- | --- | --- |
| P0.1 False absolute mute | Current source, 101-event contract tests and live JSON stay unknown | Do not claim native mute readback or a guaranteed physical gesture |
| P0.2 Implicit lighting | Offline startup/transaction tests; fresh helper remains lighting unknown | Full physical case/USB reconnect acceptance remains pending; auto-theme is separately opt-in |
| P0.3 Fabricated settings | Parser/owner suites, live ANC changes and late-result regression | Live acceptance across all exposed settings and disconnect/staleness scenarios is incomplete |
| P0.4 Restart reconciliation | Production-function restart tests and live helper restarts | Active-call restart with physical gesture verification remains pending |
| P0.5 Truthful release | Current preview, protocol caveats and public-use documentation | Final source review, exact commit identity and release metadata remain pending |

This table does not close pending P0 items. Historical audit sections in BACKLOG
and REVIEW-2026-09-08 describe their dated sources, not the final candidate.

## Before calling this stable

- Complete the remaining P0 live acceptance and record exact results.
- Extend the normal disable/re-enable check to hung detector teardown; retain
  multi-monitor, general telemetry freshness and IPC/path limits.
- Review all intended files, including untracked sources/tests/catalogs, for
  licensing, secrets and local-only artifacts. Generated helpers stay ignored.
- Run `./tests/run.sh`, `omarchy plugin validate .`, and `git diff --check` on the
  exact reviewed candidate. Static checks do not replace runtime acceptance.
- Obtain explicit permission for commit, push, tag/release and issue submission.
- Record the final commit SHA and verify the public default branch contains the
  matching manifest, entry points, catalogs, tests, README, license and preview.

## Prepared Marketplace fields

- Repository: https://github.com/PavelLizunov/omarchy-rog-cetra-control
- Name: ROG Cetra Control
- Version: 1.6.0 (candidate manifest)
- Category: Hardware
- Tags: Bar, Media, Quickshell
- Submission target: https://github.com/omacom/omarchy-plugin-marketplace/issues/new?template=submit-plugin.yml

Suggested maintainer notes:

> Controls and status for the ASUS ROG Cetra True Wireless SpeedNova USB receiver
> (0b05:1ad3, interface 3). Runs in the existing Omarchy shell with one hidapi-hidraw
> helper. Setup compiles local C sources and may install build dependencies; the
> call detector uses pactl, jq and GNU timeout. The runtime has no network calls.
> Diagnostic logs persist locally. Hardware mute state is unknown; proximity UI
> and the manual call override are not exposed. See README for installation,
> permissions, removal, dependency disclosure and remaining limitations.

Marketplace approval is listing approval, not a security review. Do not check
the submission's ownership/permission boxes on behalf of the author without
explicit authorization and reviewed asset provenance.
