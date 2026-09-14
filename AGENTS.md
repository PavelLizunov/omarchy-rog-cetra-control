# Repository Instructions & Agent Directives

## Quick Reference

- **Plugin ID:** `io.github.pavellizunov.rog-cetra-control`
- **Supported Hardware:** ASUS ROG Cetra True Wireless SpeedNova USB Receiver (`0b05:1ad3`, interface 3)
- **Primary References:**
  - `MODULES.md` — Source ownership, component interfaces, and targeted checks.
  - `HANDBOOK.md` — Current architecture, state contracts and hardware constraints.
  - `BACKLOG.md` — Authoritative release blockers, engineering debt, missing tests, and definition of done.
  - `RESEARCH.md` — Verified reverse-engineered opcodes, ASUS HAL assembly addresses, pcap analysis, and negative findings.
  - `CHANGELOG.md` — Release history and changes across versions.

Before changing runtime behavior, read `BACKLOG.md` and do not describe the
project as release-ready while any P0 blocker remains open.

Private C modules live in `daemon/` and compile through the single owner entry
point. QML section components receive the view model explicitly. Read MODULES.md
before editing; preserve tests of the complete implementation after moving code.
Historical records in `docs/archive/` and dated reviews are evidence, not current
instructions. New agent-facing procedures are written in English.

## Non-Negotiable Safety Rules

1. **Never Fuzz HID Opcodes:**
   - Do not send randomized or arbitrary HID reports to the receiver or earbuds.
   - Only send commands with verified semantics documented in `RESEARCH.md` and `HANDBOOK.md`.

2. **No Second Hidraw Reader:**
   - All USB HID communication belongs exclusively in the single long-running `cetra-watch` owner.
   - Multiple `hidraw` readers race for asynchronous interrupt reports and corrupt device state.
   - Other monitors or processes must connect via the UNIX socket (`$XDG_RUNTIME_DIR/rog-cetra-control.sock`).

3. **Microphone Mute Integrity:**
   - Never inject synthetic software microphone mute/unmute commands (e.g. `05 33` or ALSA muting) disguised as native hardware mute.
   - The headset's internal mute state is hardware-controlled by the physical earbud touch sensor and announced by the native voice prompt (*"Microphone off/on"*).
   - In-call right earbud single-tap reports (`cc 70 .. 01 01`) are gesture edges, identical for both mute directions, not absolute state. Reports can be missed; `tap_seq` counts observed reports only and must never drive a Live/Muted inference.
   - Keep microphone state `Unknown` (`microphone_state: "unknown"` in the corrected daemon contract) until a reproducible absolute hardware readback exists. Do not restore `mic_live`, manual `mic_state` resync, or a Live reset on reconnect. The plugin does not decode the voice prompt, and PCM silence is not proof of native mute. See `BACKLOG.md` P0.1 for pending implementation verification.

4. **Omarchy Lockscreen Guard (Upstream Bug #9441):**
   - Never replace binary helpers in `bin/` while `omarchy-shell lock status` indicates the screen is locked or requested.
   - Hot-reloading plugins during screen lock causes a Quickshell session lock crash in Omarchy Quattro.

5. **Omarchy Plugin Guidelines Compliance:**
   - No symlinks anywhere in the repository.
   - Zero hardcoded displayed UI hex colors in QML. SVG source masks may use white (`#fff`/`#ffffff`) and must be recolored with `Color.*`, `bar.*`, or `Style.*` at runtime. Device RGB previews are data, not decorative theme colors.
   - Strict adherence to manifest schema v1 and standard `bar-widget` lifecycle.

## Mandatory Verification Protocol

Before declaring any change complete or committing:

```bash
./tests/run.sh
omarchy plugin validate .
git diff --check
```

For runtime changes, also verify:
1. Exactly one `cetra-watch` process owns the receiver: `pgrep -a cetra-watch`.
2. Status cache is valid JSON: `cat "${XDG_RUNTIME_DIR:-/tmp}/rog-cetra-control.status"`.
3. Telemetry logging continues properly: `tail -n 10 ~/.local/state/omarchy/rog-cetra-control.log`.
