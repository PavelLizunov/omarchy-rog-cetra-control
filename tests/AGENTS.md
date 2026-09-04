# Tests Directory Instructions

## Purpose

Contains automated regression and compliance checks for the plugin.

## Execution

Run all checks from the repository root:

```bash
./tests/run.sh
```

All checks must exit with code 0 before any change is declared complete or committed.

## Check Specifications in `run.sh`

1. **Manifest Validation:**
   - Validates `manifest.json` syntax via `python3 -m json.tool`.
   - Ensures no symlinks exist in the repository tree (`find ... -type l`).
   - Runs `omarchy plugin validate .` against Omarchy schema v1.

2. **C Helper Compilation & Selftests:**
   - Compiles `cetra-status.c` and `cetra-watch.c` with `-O2 -Wall -Wextra -Werror`.
   - Runs `./cetra-status --selftest` and `./cetra-watch --selftest`.
   - Selftests verify bitmask decoding, packet parsers, command consumers, IPC buffering, and gesture edge handling.

3. **Safety & Bug Regression Guards:**
   - Verifies lockscreen guard in `./setup` to prevent upstream Quickshell crash `omacom/omarchy#9441`.
   - Verifies `signal(SIGPIPE, SIG_IGN)` presence in `cetra-watch.c` to prevent SIGPIPE termination on broken client sockets.
   - Prohibits software microphone state commands (`0x33`, `05 33`, etc.).
   - Prohibits unreliable mute-state inferences.

4. **Call Detection Filter Test:**
   - Verifies the `jq` PipeWire stream filter correctly flags communication capture streams (Chromium WebRTC, Discord, etc.) while ignoring keepalives, `pw-record`, EasyEffects, and Voxtype.

5. **Design System Color Guard:**
   - Scans all `*.qml` and `*.svg` files for hardcoded `#hex` color codes (except `#fff` in SVGs).
   - All colors must derive from Omarchy design tokens (`bar.foreground`, `Color.foreground`, `bar.urgent`, `Color.urgent`, `Color.accent`).
