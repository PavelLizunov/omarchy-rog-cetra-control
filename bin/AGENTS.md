# Bin Directory Instructions

## Purpose

`bin/` contains the local executable binaries built by `./setup` on the target system:
- `cetra-watch`: Long-lived receiver owner and UNIX socket server.
- `cetra-status`: Standalone diagnostic/testing tool for fixture playback.

## Git Rules

- Generated binaries under `bin/` (`bin/cetra-status` and `bin/cetra-watch`) are **strictly untracked** (enforced by `.gitignore`).
- Never commit binary executables to Git.
- Binary helpers are built from source on the user's machine during `./setup`.

## Daemon Lifecycle

- Exactly one `cetra-watch` instance must own `/dev/hidraw` (interface 3) at any time.
- Additional shell panels or monitors connect as clients via UNIX domain socket `$XDG_RUNTIME_DIR/rog-cetra-control.sock`.
- Status cache is published atomically to `$XDG_RUNTIME_DIR/rog-cetra-control.status`.
- Telemetry logs are appended to `$XDG_STATE_HOME/omarchy/rog-cetra-control.log`.
