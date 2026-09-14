# Changelog

## Unreleased

- Split the UI into a small entry point, view model and focused section/control
  components. Extract saved preferences and the bounded call detector from the
  shared service. Keep one HID owner and the same command/JSON contracts.
- Split native implementation into private daemon modules with a single owner
  translation unit; test snapshots cover the complete included implementation.
- Refresh source/test documentation and add MODULES.md. Record host preference
  loss during disable/re-enable and stale QML delivery as observed limitations.

- Use a private unpredictable temporary build directory in setup and report
  build/validation completion without claiming whole-plugin release readiness.

- Remove manual Request call mode, its M/Ь shortcut and manifest option. Ignore
  legacy alwaysCallContext values; retain automatic capture detection and daemon IPC.
- Allow 48 pending-settings ticks (nominal 12 seconds) for the 10-second readback
  cycle. Matching late replies clear per-setting timeout feedback without replaying writes.

- Add separate opt-in automatic theme-color updates: shared 350 ms debounce,
  session-local explicit apply authorization, duplicate suppression, and Off/Cycle
  preservation. Reset authorization on helper/receiver failure.
- Clarify unknown mute, requested call context and last-sent effect wording.
- Center battery data, emphasize percentages, increase secondary-text opacity,
  wrap toggle labels. Keep host theme tokens.
- Reconcile protocol documentation: runtime Lighting Off/commits, incomplete EQ
  mapping, two-report debounce, timestamp precision and scoped negative findings.

- Preserve locale and RGB/theme preferences through a shared settings path that
  reads the saved shell.json entry instead of trusting lagging host snapshots or
  widget injections. Verify pending preferences against disk, with a bounded
  readback timer. Cover delayed view injection in the Qt regression.
- Discard stale call detection and pending results on receiver loss.
- Restore focus to the language button after selection and wrap control labels
  within their available width. Keep Traditional Chinese requests out of the
  Simplified catalog.
- Hide auto-pause and its P/З shortcut until USB playback behavior is verified;
  preserve the current device setting and daemon research protocol.
- Document observed right-tap Play/Pause and unresolved USB proximity delivery.

- Query verified presence (`cc 12 01`) on receiver open and every 10 seconds.
  Charging (`cc 12 08`) remains passive. Both report families expose nullable
  decoded fields with 30-second expiry and preserve historical raw bytes.
- Gate UI controls on fresh per-earbud presence after the first observation;
  retain battery-only fallback before it. The C daemon's `connected` field is
  unchanged. Show presence/charging beside the three battery columns and clear
  stale UI telemetry and pending mode requests on helper errors or receiver loss.
- Replace fabricated ANC level/Adaptive/voice/proximity defaults with unknown
  until valid readback, strict report domains, and 30-second TTL. Invalidate on
  reset, invalid reports and earbud unavailability; reported absence vetoes late
  replies. Query verified `2b/2c/28/26` round robin using the monotonic clock,
  one periodic request every 2500 ms, each field once per nominal 10-second cycle
  at most. First eligible query is `2b`; preserve the deadline across availability
  changes to avoid bursts. These are plugin policies, not ASUS timing guarantees.
- Share settings pending/confirmed state without optimistic selections. Matching
  readback clears requests; forty-eight 250 ms UI scheduler ticks yield unconfirmed
  feedback otherwise, not a wall-clock deadline. Unknown toggles request On;
  manual ANC level selection disables On/Unknown Adaptive. Voice protocol tokens
  remain `english`/`chinese`/`sound`, separate from interface catalogs.
- Add a collapsible RGB palette with saved integer channels, a theme-color
  toggle, and explicit Apply. Selection edits alone do not send HID commands;
  theme updates require the separate session-gated opt-in described above.
  Apply retains Static/Breathing/Strobing or switches Off/Cycle/Unknown to Static.
- Keep lighting unknown until a complete explicit transmission succeeds. Only
  that session's successful preference is eligible for reconnect replay; a
  partial failure cannot replace it and may leave hardware partly changed.
- Integrate local JSON interface catalogs, a `locale` setting, exact/base/English
  fallback, named placeholders, and RTL panel mirroring. Bundle English, Russian,
  German, French, Spanish, Italian, Portuguese, Simplified Chinese, Japanese and
  Korean. Hardware voice prompts remain English/Chinese/Beeps.
- Move watcher, call detector, and mode pending state into the shared shell
  service. Reconcile call context on watcher start and bound detector failures;
  service destruction cleanup remains best effort.
- Implement panel-wide keyboard traversal through the standard single shortcut
  path, visible/enabled control collection, focus scrolling and single-owner
  toggle rows. Offline keyboard fixtures do not verify rendered Qt event routing.
- Strictly validate non-lighting IPC values and trailing arguments; support
  LF/CRLF framing while rejecting embedded NUL/CR. Owner `send_line` completes
  partial sends with bounded interruption retries, disconnecting on backpressure.
  Mirror forwarding, broader backpressure and unsafe fixed `/tmp` runtime fallback
  remain open.
- Restrict telemetry to the elected owner with early umask, no-follow opens,
  owner/type/single-link checks and mode `0600`. Accepted older `0644` logs are
  converted on first owner write, not retroactively at deployment. Validate log
  directory leaves/creation parents, not every existing path ancestor; retain
  remaining logging I/O, opt-out and live acceptance work. Use unique owner-only
  `mkostemp` cache replacement without claiming whole-runtime-path safety.
- Add offline suites for presence scheduling/expiry, lighting, shared state,
  calls, localization, settings, IPC, logging and keyboard logic. Current evidence
  and remaining acceptance are in RELEASE.md; test outputs report exact counts.
- Disclose runtime/test dependencies, persistent telemetry and 5 MiB log rotation,
  saved preferences, and runtime versus installation network behavior. Preserve
  the historical `REVIEW-2026-09-08.md`; current acceptance is in RELEASE.md.
  Replace the unsupported fixture-prefixed
  shell restart with an offline `cetra-status` JSON-printing test-helper example,
  not a UI preview. No claim that all bugs or keyboard issues are fixed.
- Correct the erroneous 1.5.0 claim of verified ADC-based absolute microphone
  mute detection. PCM silence observations are research evidence only, not an
  implemented production readback; audio-graph mute or gating can also produce
  silence. Follow the headset's native voice prompt.
- Document the current `Unknown` microphone UI and daemon contract:
  `microphone_state: "unknown"`, edge-count-only `tap_seq`, removal of inferred
  `mic_live` and the manual `mic_state` command, and guarded access to optional
  gesture byte 7. BACKLOG P0.1 records earlier offline evidence separately from
  deployment and physical acceptance.

## 1.5.0 - 2026-09-02

- Erroneously reported verified hardware-synchronized microphone Live/Muted detection via ADC silence gating and physical `cc 70` tap events. This was not an implemented absolute readback; see the Unreleased correction.
- Added a Live/Muted icon in the top bar during calls with theme-colored urgent alerts, but its inferred state was not hardware-confirmed; see the Unreleased correction.
- Add ANC Level controls (Low, Mid, High) and Smart Adaptive ANC toggle with verified readbacks `cc 12 2b` and `cc 12 2c`.
- Add Aura RGB Lighting controls (Off, Cycle, Static, Breathing, Strobing) using verified protocol `cc 51 28` and commit report `cc 50 55`.
- Add Voice Prompt language selector (English, Chinese, Beeps) with verified readback `cc 12 28`.
- Add In-Ear Detection (Auto-Pause) toggle with verified readback `cc 12 26`.
- Claimed an extended multi-phase query schedule. That claim does not describe
  the audited implementation: ANC level, Adaptive ANC, voice prompt, and
  proximity were not periodically queried there. The later presence and bounded
  settings schedules are documented under Unreleased; BACKLOG P0.3 records
  offline implementation separately from pending physical acceptance.

## 1.4.0 - 2026-09-02

- Add native call-context integration for the headset's right-earbud mute tap.
- Preserve the headset's own muted/unmuted voice prompts.
- Keep microphone state read-only: do not present software mute buttons or an
  inferred `Live`/`Muted` state without a reliable absolute hardware readback.
- Automatically switch between Telephony call controls and normal media gestures.
- Keep a single reconnecting HID owner behind an atomic runtime lock, with
  post-case Telephony restore and per-panel call-context aggregation.
- Reject local helper installation while the Omarchy lockscreen is active to
  avoid upstream hot-reload crash `omacom/omarchy#9441` on affected releases.
- Detect Chromium, Firefox, Electron, and communication-role call capture while
  excluding EasyEffects, `pw-record`, Voxtype, and recognition keepalives.
- Document official ASUS HAL findings, raw capture evidence, ruled-out command
  paths, and the remaining reverse-engineering plan.

## 1.3.0 - 2026-09-01

- Rename the project to ROG Cetra Control.
- Move to the final plugin ID `io.github.pavellizunov.rog-cetra-control`.
- Reserve the broader project scope for microphone and gesture controls.

## 1.2.1 - 2026-09-01

- Add marketplace preview.
- Clarify the hardware-control disclosure in the panel.

## 1.2.0 - 2026-09-01

- Add verified Off, ANC, and Ambient mode switching.
- Read back the active mode after every command.
- Add mouse controls and O/N/A keyboard shortcuts.

## 1.1.0 - 2026-09-01

- Add theme-aware symbolic SVG icon.
- Add horizontal and vertical bar layouts.
- Add left, right, and case battery panel.
- Add configurable refresh and visibility settings.
- Add multi-monitor lock and runtime cache.
- Add HID interface selection, retries, diagnostics, watchdog, and debounce.
- Add source-only setup and regression tests.
