# Bin Directory Instructions

## Purpose

`bin/` contains the local executable binaries built by `./setup` on the target system:
- `cetra-watch`: Long-lived receiver owner and UNIX socket server.
- `cetra-status`: Bounded settings reader and offline fixture diagnostic tool; no HID.
- `cetra-peak`: Optional audio-only libpulse peak client; never accesses HID.

## Git Rules

- Generated binaries under `bin/` are **strictly untracked** (enforced by `.gitignore`).
- Never commit binary executables to Git.
- Binary helpers are built from source on the user's machine during `./setup`.
- `cetra-watch.c` includes private implementation modules from `daemon/`; retain
  that directory in clean builds. Do not compile those headers independently.

## Daemon Lifecycle

- Exactly one `cetra-watch` instance must own `/dev/hidraw` (interface 3) at any time.
- Shell views share the manifest service. Additional CLI clients use the UNIX
  domain socket `$XDG_RUNTIME_DIR/rog-cetra-control.sock`.
- Status cache is published atomically to `$XDG_RUNTIME_DIR/rog-cetra-control.status`.
- Telemetry logs use `${XDG_STATE_HOME:-$HOME/.local/state}/omarchy/rog-cetra-control.log`.
