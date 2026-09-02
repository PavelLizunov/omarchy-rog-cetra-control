# Contributing

Bug reports and pull requests are welcome.

Before submitting a change, run:

```bash
./tests/run.sh
```

Hardware reports should include:

- the output of `lsusb -d 0b05:`;
- the receiver product name;
- the Omarchy version;
- whether the USB receiver or Bluetooth mode was used.

Do not add write commands for unknown HID opcodes. Device-control changes need
captured protocol evidence and explicit testing on supported hardware. The
documented noise-control command is `cc 41 08 00 00 MODE` as a 64-byte HID
Output Report.

Runtime device access belongs in `cetra-watch.c`. Do not add a second process
that opens the receiver: multiple hidraw readers race and can consume each
other's asynchronous events.

Read [RESEARCH.md](RESEARCH.md) before investigating microphone or gesture
commands. It records the official HAL addresses, captures already checked, and
the exact unresolved tests. Do not send `cc 41 0b` values until their semantics
are proven statically or through an official Armoury Crate capture.
