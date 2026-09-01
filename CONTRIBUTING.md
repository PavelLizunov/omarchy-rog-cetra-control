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
