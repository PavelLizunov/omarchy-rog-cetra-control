# ROG Cetra Control for Omarchy

Battery status, noise control, Aura lighting and voice-prompt settings for ASUS
ROG Cetra True Wireless SpeedNova. USB controls require receiver `0b05:1ad3`,
interface 3. Bluetooth provides read-only battery, ANC and audio-route status;
other Cetra models are not supported.

[Releases and release notes](https://github.com/PavelLizunov/omarchy-rog-cetra-control/releases)

Runs inside the existing Omarchy Quattro shell on Linux.

## Install

Plugins run unsandboxed with your user permissions. This plugin uses native
helpers and persistent local diagnostics. Read the
[security and privacy notes](docs/user-guide.md#security-and-privacy) first.

```bash
repo=https://github.com/PavelLizunov/omarchy-rog-cetra-control.git
id=io.github.pavellizunov.rog-cetra-control
omarchy plugin add "$repo" --yes
~/.config/omarchy/plugins/"$id"/setup
omarchy plugin enable "$id" --section right
```

Setup compiles four helpers, runs their offline selftests and validates the
folder. It requires an explicitly unlocked Omarchy session. Marketplace
installation does not run setup automatically.

Runtime dependencies include Omarchy Quattro, Quickshell, hidapi, libpulse,
BlueZ and a PulseAudio-compatible audio server. Setup additionally needs bash,
jq, GNU timeout, a C compiler and pkg-config. See the
[complete dependency notes](docs/user-guide.md#install).

## Use

Click the bar icon to open Sound: charge, noise control and microphone signal.
Right-click or use the wheel to cycle noise modes over USB. Device contains
Settings and Color tabs. The header language button opens the interface-language
chooser without moving the header or tabs.

- Battery percentages are last-reported values. Unknown stays unknown.
- USB enables ANC, Ambient, lighting and headset voice settings.
- Bluetooth selection observes an existing audio identity; it never pairs,
  connects or switches profiles. Bluetooth ANC is read-only.
- The microphone signal meter is optional and defaults Off. Signal or silence
  does not establish native mute; follow the headset's voice prompt.
- Tab and Shift+Tab traverse controls. Enter or Space activates. Escape closes
  an editor first, then the panel. Changing tabs sends no headset command.

The [user guide](docs/user-guide.md) preserves detailed controls, keyboard
shortcuts, Bluetooth retry/freshness rules, lighting drafts, languages,
preferences and known limitations.

## Update and remove

```bash
id=io.github.pavellizunov.rog-cetra-control
omarchy plugin update "$id"
~/.config/omarchy/plugins/"$id"/setup
```

Verify an actual visible change after updates; a reload log alone is insufficient.
Back up the plugin's inline preferences before disabling: the tested host could
remove them during disable/re-enable. See
[update and removal guidance](docs/user-guide.md#update-and-remove) before running:

```bash
omarchy plugin disable "$id"
omarchy plugin remove "$id"
```

Diagnostics remain after removal. No system service is installed.

## Development

Start with [DEVELOPMENT.md](DEVELOPMENT.md) for development boundaries and
hardware safety. Local harness entry files are not distributed with the plugin.
The installed plugin is a separate copy; this checkout is not automatically deployed.

```bash
./tests/run.sh
omarchy plugin validate .
git diff --check
```

Offline fixtures do not replace live hardware or installed-shell acceptance.
The aggregate includes panel interaction checks using private Xvfb and Mesa
OpenGL. Install the test dependency `xorg-server-xvfb`; no production display or
hardware is used. [Test coverage and dependencies](tests/README.md) describe
the limits. [MODULES.md](MODULES.md) maps source owners;
[HANDBOOK.md](HANDBOOK.md) describes state and process ownership;
[BACKLOG.md](BACKLOG.md) tracks release blockers.
[RESEARCH.md](RESEARCH.md) contains dated protocol evidence, not instructions.

## License

Source is [MIT](LICENSE). This community project is not affiliated with ASUS.
ROG, Cetra, SpeedNova and ASUS are ASUSTeK trademarks.
