# ROG Cetra Battery for Omarchy

Shows left, right, and case battery levels for ROG Cetra True Wireless
SpeedNova through its USB receiver.

Supported receiver: `0b05:1ad3`.

The reader sends the read-only status request `cc 12 07` to the receiver.
The response fields are:

- byte 6: left earbud battery
- byte 7: right earbud battery
- byte 8: case battery
- `255`: component unavailable or in the case

## Install

Install the plugin from GitHub, build the local HID reader, and enable it:

```bash
omarchy plugin add https://github.com/PavelLizunov/omarchy-rog-cetra-battery.git --yes
~/.config/omarchy/plugins/io.github.pavellizunov.rog-cetra-battery/setup
omarchy plugin enable io.github.pavellizunov.rog-cetra-battery --section right
```

No per-headset calibration is required. The left/right case tests were only
used once to document the receiver protocol.

## Omarchy integration

- Uses the Omarchy plugin manifest schema version 1.
- Uses theme colors and spacing from `qs.Commons` and panel components from
  `qs.Ui`, matching first-party Omarchy widgets.
- Supports horizontal and vertical bars. The vertical bar uses an icon-only
  compact layout.
- Uses a symbolic SVG recolored to the active theme instead of fixed colors.
- Uses a shared lock and short-lived cache so multiple monitors do not poll the
  same HID receiver concurrently.
- Preserves the last valid values across transient USB read failures.
- Exposes refresh and visibility settings through the Omarchy widget settings
  schema.

The `setup` script is intentionally manual. `omarchy plugin add` clones and
validates third-party plugins but does not execute their installation scripts.

## Verify

```bash
~/.config/omarchy/plugins/io.github.pavellizunov.rog-cetra-battery/tests/run.sh
```

The checks validate the Omarchy manifest, compile with warnings treated as
errors, run the protocol parser self-test, and reject hard-coded display colors.

For UI development without hardware, launch Omarchy Shell with a fixture:

```bash
CETRA_STATUS_FIXTURE='{"status":"ok","receiver":true,"connected":true,"state":5,"left":91,"right":98,"case":100}' omarchy restart shell
```

## Update

```bash
omarchy plugin update io.github.pavellizunov.rog-cetra-battery
~/.config/omarchy/plugins/io.github.pavellizunov.rog-cetra-battery/setup
```

## Remove

```bash
omarchy plugin disable io.github.pavellizunov.rog-cetra-battery
omarchy plugin remove io.github.pavellizunov.rog-cetra-battery
```

The plugin installs no system service and writes no persistent device data.
The generated `bin/cetra-status` helper lives inside the plugin directory and
is removed together with the plugin.

## Security and privacy

- Reads only the USB HID device `0b05:1ad3`.
- Sends the read-only status request `cc 12 07`.
- Does not use the network.
- Does not change firmware, audio, microphone, or headset settings.
- Does not collect serial numbers or other identifiers.
- `setup` may install `base-devel`, `hidapi`, and `pkgconf` through
  `omarchy pkg add` when they are missing.

## Compatibility

Tested with ROG Cetra True Wireless SpeedNova receiver `0b05:1ad3` on Omarchy
Quattro. Other Cetra models and hardware revisions may use different USB IDs or
response layouts and are not currently supported.

## Trademark notice

ROG, Cetra, SpeedNova, and ASUS are trademarks of ASUSTeK Computer Inc. This
community project is not affiliated with or endorsed by ASUS.

## License

MIT. See `LICENSE`.
