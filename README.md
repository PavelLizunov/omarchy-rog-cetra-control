# ROG Cetra Control for Omarchy

Shows left, right, and case battery levels for ROG Cetra True Wireless
SpeedNova through its USB receiver, switches Off, ANC, and Ambient modes, and
automatically activates the headset's built-in call microphone gesture.

Supported receiver: `0b05:1ad3`.

The reader sends the read-only status request `cc 12 07` to the receiver.
The response fields are:

- byte 6: left earbud battery
- byte 7: right earbud battery
- byte 8: case battery
- `255`: component unavailable or in the case

Noise control uses the 64-byte HID Output Report `cc 41 08 00 00 MODE`, where
`0` is Off, `1` is ANC, and `2` is Ambient. The selected mode is verified with
the readback request `cc 12 25`.

Call context uses the standard Telephony HID output report exposed by the
headset itself:

- `05 31`: call context active
- `05 00`: media context active

This activates the headset's native call controls. The right-earbud tap then
toggles microphone mute inside the headset and plays its normal voice prompt.
It does not mute PipeWire, PulseAudio, EasyEffects, or the ALSA mixer.

The Telephony reports do not emulate a physical tap and do not play the
`microphone off/on` voice prompt. No host command equivalent to the physical
right-earbud tap has been confirmed, so the panel intentionally does not offer
software `Live` or `Muted` buttons.

The receiver has no confirmed absolute mute-state readback. A vendor packet
`cc 70 00 00 00 01 01` was observed after some physical taps, but it was absent
from another captured tap that played `microphone off`, and ASUS HAL 1.3.95.0
does not parse its `0x70` payload. The panel therefore does not infer or display
`Live`/`Muted`; the headset's own voice prompt is the authoritative state.
See [RESEARCH.md](RESEARCH.md) for captures, HAL addresses, negative findings,
and the remaining verification plan.

The Omarchy widget automatically enables the headset's Telephony call context
while Discord, Steam, Telegram, Zoom, a Chromium- or Firefox-based call, or
another communication capture stream is using the microphone. Technical
EasyEffects, `pw-record`, Voxtype, and recognition keepalive streams are ignored.
Outside a call, the right-earbud tap remains a media gesture.

## Install

Install the plugin from GitHub, build the local HID reader, and enable it:

```bash
omarchy plugin add https://github.com/PavelLizunov/omarchy-rog-cetra-control.git --yes
~/.config/omarchy/plugins/io.github.pavellizunov.rog-cetra-control/setup
omarchy plugin enable io.github.pavellizunov.rog-cetra-control --section right
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
- Uses one long-lived `cetra-watch` owner for battery, ANC, and call context;
  additional monitors connect through a private runtime socket.
- Preserves the last valid values across transient USB read failures.
- Exposes display and visibility settings through the Omarchy widget settings
  schema.

The `setup` script is intentionally manual. `omarchy plugin add` clones and
validates third-party plugins but does not execute their installation scripts.
The script refuses to replace generated helpers while the Omarchy lockscreen is
active. Omarchy 4.0.2 can reload every plugin service after a local plugin file
changes and strand the session lock; this is tracked upstream as
[`omacom/omarchy#9441`](https://github.com/omacom/omarchy/issues/9441).

## Verify

```bash
~/.config/omarchy/plugins/io.github.pavellizunov.rog-cetra-control/tests/run.sh
```

The checks validate the Omarchy manifest, compile with warnings treated as
errors, run protocol self-tests, and reject hard-coded display colors.

For UI development without hardware, launch Omarchy Shell with a fixture:

```bash
CETRA_STATUS_FIXTURE='{"status":"ok","receiver":true,"connected":true,"left":91,"right":98,"case":100,"mode":"anc","call_context":true}' omarchy restart shell
```

## Update

```bash
omarchy plugin update io.github.pavellizunov.rog-cetra-control
~/.config/omarchy/plugins/io.github.pavellizunov.rog-cetra-control/setup
```

## Remove

```bash
omarchy plugin disable io.github.pavellizunov.rog-cetra-control
omarchy plugin remove io.github.pavellizunov.rog-cetra-control
```

The plugin installs no system service and writes no persistent device data.
The generated `bin/cetra-status` and `bin/cetra-watch` helpers live inside the
plugin directory and are removed together with the plugin.

## Security and privacy

- Reads only the USB HID device `0b05:1ad3`.
- Sends status requests `cc 12 07` and `cc 12 25`.
- Sends `cc 41 08` only when the user explicitly changes noise control.
- Sends standard Telephony HID call-context reports while a real capture stream
  is active.
- Does not use the network.
- Does not change firmware, audio routing, system microphone mute, or unrelated
  headset settings.
- Does not collect serial numbers or other identifiers.
- Uses owner-only runtime files and a private Unix socket under
  `$XDG_RUNTIME_DIR`.
- `setup` may install `base-devel`, `hidapi`, and `pkgconf` through
  `omarchy pkg add` when they are missing.

## Compatibility

Tested with ROG Cetra True Wireless SpeedNova receiver `0b05:1ad3` on Omarchy
Quattro. Other Cetra models and hardware revisions may use different USB IDs or
response layouts and are not currently supported.

## Renamed from ROG Cetra Battery

Versions through `1.2.1` used the plugin ID
`io.github.pavellizunov.rog-cetra-battery`. The project was renamed before its
marketplace submission because it now covers device controls as well as battery
status. Remove the old plugin ID before installing the new one.

## Trademark notice

ROG, Cetra, SpeedNova, and ASUS are trademarks of ASUSTeK Computer Inc. This
community project is not affiliated with or endorsed by ASUS.

## License

MIT. See `LICENSE`.
