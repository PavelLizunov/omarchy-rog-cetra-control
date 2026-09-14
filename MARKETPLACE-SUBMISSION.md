# Marketplace submission draft — 1.7.0

Prepared only. Do not submit until the complete reviewed candidate is committed
and available on the public default branch. Record that commit in the issue notes.

## Repository URL

https://github.com/PavelLizunov/omarchy-rog-cetra-control

## Category

Hardware

## Tags

Bar, Media, Quickshell

## Maintainer notes

ROG Cetra Control 1.7.0 supports the ASUS ROG Cetra True Wireless SpeedNova USB
receiver (0b05:1ad3, interface 3). It provides battery/status, ANC, lighting, voice
settings and optional microphone signal level in the existing Omarchy shell.

One source-built cetra-watch owns HID. cetra-peak is an optional audio-only libpulse
client; cetra-status performs bounded settings readback and offline fixtures.
Setup compiles local sources and may install base-devel, hidapi, libpulse and
pkgconf. It requires jq/coreutils and an explicitly unlocked running shell.
Runtime requires a private XDG_RUNTIME_DIR and the installed Quickshell PipeWire API.

The plugin runs unsandboxed with user permissions. It has no application network
requests or PCM persistence. Local diagnostics are disclosed and can be disabled
with CETRA_DIAGNOSTICS=0. Hardware mute is always Unknown. Continuous background
capture can suppress native media taps; no other application's configuration is
changed. README documents installation, update, removal and accepted limitations.

Regression tests, scoped QML lint, isolated setup and named sanitizer suites pass.
Live results and limits are in ACCEPTANCE-2026-09-14.md. Suspend/resume and a second
monitor are explicitly untested. Self-review is not independent review.

Publication commit: **fill after authorized commit/push; never use the 1.6.0 SHA**.

## Author checklist

The author must confirm the official form's checkboxes personally, including
permission to submit the plugin and preview assets. They are deliberately not
pre-checked here. The existing preview predates the microphone meter; it is labeled
historical in README. A preview is optional under the publishing guide.

Form verified during preparation:
https://github.com/omacom/omarchy-plugin-marketplace/issues/new?template=submit-plugin.yml

Marketplace structural/security-baseline results and maintainer approval have
not been obtained for this local candidate.
