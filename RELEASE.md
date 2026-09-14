# 1.7.0 release preparation

## Published baseline

[v1.6.0](https://github.com/PavelLizunov/omarchy-rog-cetra-control/releases/tag/v1.6.0)
is a GitHub pre-release targeting
`d5a687b5ff2a4cd9482a83ea2acb0773d6d8d7c9`, published 2026-09-14.
Do not move existing v1.5.0/v1.6.0 tags. The current local changes are not in that
release and need a new version and explicitly authorized publication.

## Candidate 1.7.0

The user accepted documented limitations and requested release preparation.
Release notes: [RELEASE-NOTES-1.7.0.md](RELEASE-NOTES-1.7.0.md).
Submission draft: [MARKETPLACE-SUBMISSION.md](MARKETPLACE-SUBMISSION.md).
Publication and a final Git commit are still pending; no existing tag is changed.

- Audio-only libpulse peak helper, opt-in and shared by all views.
- Reactive PipeWire endpoint/path observation; processing/keepalive streams alone
  do not authorize capture or call context. No periodic pactl/jq pipeline.
- Unknown native mute; no synthetic media/microphone actions from vendor taps.
- Bounded owner stdout/mirror queues, private runtime root and checked lock file.
- Battery/mode freshness flags with a 30-second expiry and UI domain validation.
- Explicit unlocked-only setup, same-directory binary staging, repeat-install
  preservation and per-helper refresh on binary replacement.
- Bounded host-settings reader via cetra-status; watch-only FileView.
- Corrected diagnostic wording and documented continuous-capture media-key limit.

## Evidence and remaining acceptance

The owning record is [ACCEPTANCE-2026-09-14.md](ACCEPTANCE-2026-09-14.md), including
source identity, host versions, mixed-workload resource measurements and limits.
[COMPLETION-PLAN.md](COMPLETION-PLAN.md) tracks the user-approved work.

| Area | Observed result | Remaining boundary |
| --- | --- | --- |
| Native mute | Unknown throughout tests; user confirmed Off/On prompts in real Discord trials | No absolute hardware readback exists |
| Calls / meter | Real call entry, exit and shell restart passed through EasyEffects; ordinary recording did not request a call | Other communication apps and ambiguous graphs are not universally verified |
| USB / case | Same owner recovered USB; both/left/right availability trials passed | Colored lighting replay and suspend/resume need marked acceptance |
| Controls | All ANC modes/levels/Adaptive, voice prompts and physical lighting effects exercised | Failure-injection is offline; not physical firmware fault testing |
| Preferences | RU/EN/RU and meter opt-out/in survived close/reopen after bounded-reader delivery | Host disable/re-enable can delete inline preferences |
| Resources | 30-minute mixed workload: stable FD counts, no sustained RSS growth; sanitizers passed named suites | Not proof against every leak or filesystem/kernel stall |
| QML | All 19 files pass scoped semantic lint with verified host declarations | Lexical-access style advice and unused host enum exceptions documented |
| Presentation | Localized keys/placeholders and Qt label fixtures pass | Full screen-reader, RTL, contrast and multimonitor acceptance incomplete |
| Publication | Public repository, version 1.7.0, root manifest, README and MIT license present | Exact commit and submission approval pending; no independent review claimed; author confirms submission rights |

## Known compatibility boundaries

Continuous capture can suppress native Play/Pause on the tested headset path even
when call_context is false and the meter is absent. A marked keepalive on/off/on
trial reproduced this. The other service was restored, and the user explicitly
excluded changing other projects. See RESEARCH.md; no software gesture substitute
is enabled.

The runtime requires a private XDG_RUNTIME_DIR. Logging/cache filesystem I/O can
still stall on pathological storage; diagnostic writes can be disabled with
CETRA_DIAGNOSTICS=0. Ancestor checks are not a descriptor-relative guarantee
against concurrent same-user directory replacement.

## Candidate gates

```bash
./tests/run.sh
omarchy plugin validate .
git diff --check
```

These execute the working tree, not an arbitrary Git index. Recheck the exact
candidate after relevant changes, including untracked files. Verify installed
binary hashes and visible UI when runtime delivery changes. Generated helpers
remain ignored; no symlinks or binary executables may be committed.

The setup test compiles three real helpers in an isolated source directory,
preserves their inodes/mtime on a repeat run and retains them on compile failure.
Host commands are stubbed in that test; it does not prove a live locked update.

## Marketplace preparation

- Repository: https://github.com/PavelLizunov/omarchy-rog-cetra-control
- Name: ROG Cetra Control
- Candidate version: 1.7.0; publication commit pending
- Proposed category: Hardware
- Proposed tags: Bar, Media, Quickshell
- Form: https://github.com/omacom/omarchy-plugin-marketplace/issues/new?template=submit-plugin.yml

Maintainer disclosure: source-built HID owner plus optional audio peak and bounded
settings helpers; setup may install build dependencies. Runtime reads local
PipeWire/Pulse metadata and audio peaks, with no application network requests.
Diagnostic logs are local. Hardware mute is unknown; scope is the SpeedNova USB
receiver 0b05:1ad3, interface 3.

Marketplace now runs a security baseline as well as structural validation. Setup
and package-management capabilities can require maintainer review. This document
does not claim that the server-side scan ran or that listing approval is granted.
Commit, push, release and submission remain separate user-authorized actions.
