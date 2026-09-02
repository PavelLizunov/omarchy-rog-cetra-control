# Repository Instructions

## Start here

Read `RESEARCH.md` before changing microphone, gesture, HID, or USB behavior.
It contains the verified protocol bytes, official ASUS HAL addresses, artifact
hashes, ruled-out paths, and the exact remaining investigation plan.

## Safety rules

- Do not fuzz HID opcodes or values on the headset.
- Do not send `cc 41 0b` until its value semantics are proven from official code
  or an annotated Armoury Crate capture.
- Do not add software microphone `Live`/`Muted` controls or persistent mute state
  without a reproducible absolute hardware readback.
- Do not treat `05 31`, `05 33`, or `05 00` as microphone state commands.
- Keep all receiver access in the single long-lived `cetra-watch` owner. Never
  add a second hidraw reader.
- Do not replace plugin helpers while `omarchy-shell lock status` reports
  `locked`, `requested`, or `secure` as true.
- Do not commit, push, publish, or create a release unless the user explicitly
  requests it.

## Scope and style

- Prefer the smallest change supported by captured protocol evidence.
- Preserve the existing Omarchy visual language and desktop/mobile bar behavior.
- Keep generated binaries under `bin/` untracked.
- Put new protocol evidence and negative findings in `RESEARCH.md` before
  changing public claims in `README.md`.

## Required verification

Run these before reporting a code change complete:

```bash
./tests/run.sh
omarchy plugin validate .
git diff --check
```

For runtime changes, also verify that exactly one `cetra-watch` process owns the
receiver and that battery/ANC hardware readback still works. For shell reloads,
check for new Quickshell coredumps before reporting success.
