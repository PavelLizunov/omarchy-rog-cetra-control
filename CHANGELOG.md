# Changelog

## 1.5.0 - 2026-09-02

- Add verified hardware-synchronized microphone Live/Muted detection via ADC silence gating and physical `cc 70` tap events.
- Display real-time microphone status icon (󰍬 Live / 󰍭 Muted) in the top bar during calls with theme-colored urgent alerts.
- Add ANC Level controls (Low, Mid, High) and Smart Adaptive ANC toggle with verified readbacks `cc 12 2b` and `cc 12 2c`.
- Add Aura RGB Lighting controls (Off, Cycle, Static, Breathing, Strobing) using verified protocol `cc 51 28` and commit report `cc 50 55`.
- Add Voice Prompt language selector (English, Chinese, Beeps) with verified readback `cc 12 28`.
- Add In-Ear Detection (Auto-Pause) toggle with verified readback `cc 12 26`.
- Update `cetra-watch` daemon with extended multi-phase hardware query schedule.

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
