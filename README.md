# ROG Cetra Control for Omarchy

Battery status, noise control, Aura lighting and headset voice-prompt settings
for ASUS ROG Cetra True Wireless SpeedNova over its USB receiver (`0b05:1ad3`,
interface 3). Bluetooth and other Cetra models are not supported.

![ROG Cetra Control panel](preview.png)

The manifest is a **1.6.0 candidate**. Release acceptance remains open in
[BACKLOG.md](BACKLOG.md) and [RELEASE.md](RELEASE.md). The preview shows the same
controls before the internal module split; no telemetry is fabricated.

## Install

Plugins run unsandboxed with your user permissions inside the existing Omarchy
shell. This plugin uses a native receiver helper and persistent local diagnostics.
Review [Security and privacy](#security-and-privacy) before installing.

Runtime dependencies: Omarchy Quattro/Quickshell, `hidapi` (hidraw backend),
`bash`, `pactl`, `jq` and GNU `timeout`. A PulseAudio-compatible audio server is
needed for capture detection. Building needs a C compiler and `pkg-config`.
The manual setup script can install `base-devel`, `hidapi` and `pkgconf`; it
does not install every runtime or test dependency.

```bash
omarchy plugin add https://github.com/PavelLizunov/omarchy-rog-cetra-control.git --yes
~/.config/omarchy/plugins/io.github.pavellizunov.rog-cetra-control/setup
omarchy plugin enable io.github.pavellizunov.rog-cetra-control --section right
```

The Marketplace clones source; it does not execute setup automatically. Setup
compiles both helpers, runs their offline selftests and validates the folder.
It refuses binary replacement while Omarchy reports the screen locked.

## Use

Click the bar icon to open the panel. Right-click or use the wheel to cycle noise
modes. The vertical bar is icon-only; the horizontal bar can show the lowest
reported earbud percentage.

- **Noise control:** Off, ANC and Ambient. In ANC, select Low/Mid/High or Adaptive.
  A manual level requests Adaptive Off when its current state is On or Unknown.
- **Battery:** percentages are last-reported values. Missing data is not proof of
  case placement. Detailed availability/charging observations are in the tooltip.
- **Voice prompts:** English, Chinese or Beeps; these are headset settings,
  separate from the interface language.
- **Keyboard:** Tab/Shift+Tab traverse controls, arrows move focus, Enter/Space
  activate, Escape closes. O/N/A select noise mode; 1/2/3 select ANC level.
  Russian-layout equivalents are supported for O/N/A. RGB arrows edit the channel;
  Enter focuses Apply.

Controls require receiver/earbud availability. Once presence has been observed,
stale battery values cannot re-enable controls. Pending settings wait up to 48
250 ms scheduler ticks (nominally 12 seconds), allowing the periodic ten-second
readback cycle. A late matching reply clears the error without repeating a write.
The selected state is readback, not an optimistic click result.

### Lighting

Open Device settings → Lighting → Color palette. Select the theme accent or
integer RGB channels (0–255), then press Apply or choose an effect.

- Changing RGB/theme selection saves preferences only.
- Apply retains Static/Breathing/Strobing; from Off/Cycle/Unknown it selects Static.
- **Update with theme changes** is separate and defaults Off. With it enabled,
  explicitly apply a colored effect once per helper/connection session. Subsequent
  accent changes are coalesced for 350 ms and sent by the shared service.
- Enabling auto-theme explicitly reapplies an existing colored effect. Off and
  Cycle are never replaced automatically; identical payloads are deduplicated.
- Helper/receiver reset disarms automatic theme updates. Saved permission alone
  sends nothing at startup; apply a colored effect again to resume.
- The native owner may replay its last successful explicit preference on reconnect
  within the same owner session. Restart forgets that preference.

The preview is your selection. “Last sent” means a complete helper transmission,
not physical color readback. A partial USB write can change a zone even if the
transaction fails. The official Off/duplicate-commit sequence still needs capture
verification; the implemented sequence is recorded in RESEARCH.md.

### Microphone and calls

**Native microphone mute is unknown to the plugin.** Follow the headset voice
prompt. The plugin does not decode it, record PCM, infer mute from tap parity or
offer synthetic software mute as a native hardware control.

Capture metadata drives automatic call-context requests. Two inactive polls or
three nonpositive results clear detection; the subprocess group is bounded by
GNU timeout. Application-name matching can mistake browser recording for a call.
The JSON `call_context` value is a requested context, not confirmed tap assignment.

The manual Request call mode control and M/Ь shortcut were removed because their
benefit outside a real call was unverified. Legacy `alwaysCallContext` is ignored.
The proximity/auto-pause control and P/З were also removed: enabling its sensor
setting did not establish PC playback pause over USB. Neither removal changes the
headset's existing proximity setting. Research commands remain in daemon IPC.

User trials found effective mute could be lost across left-earbud availability
changes without another tap. Read the dated [protocol research](RESEARCH.md)
before relying on behavior across case transitions. These are observations, not
a firmware guarantee or absolute mute readback.

## Interface language and preferences

Use the language button in the header: System, English, Russian, German, French,
Spanish, Italian, Portuguese, Simplified Chinese, Japanese or Korean. System is
the default. Manifest settings labels remain English. All catalog keys and
placeholders are checked; fluent-human review of every locale remains pending.

Fallback is exact locale → compatible base → English → source text. Traditional
Chinese requests use `zh-Hant`/English rather than the Simplified `zh` catalog.
An explicit script takes precedence over region. See [locales/README.md](locales/README.md).

UI preferences live in the plugin's inline entry in `~/.config/omarchy/shell.json`.
Writes use the scoped Omarchy API. `CetraPreferences.qml` reads the saved entry
through FileView because host snapshots and widget injections can be stale.
Accepted writes override older completions until readback or a three-second
reload; malformed reads preserve last valid state.

**Disabling/re-enabling can reset inline preferences in the tested Omarchy host.**
Back up your plugin entry before doing so. Ordinary popup close/reopen does not
disable the plugin. The plugin does not maintain a second hidden settings file.

## Update and remove

```bash
omarchy plugin update io.github.pavellizunov.rog-cetra-control
~/.config/omarchy/plugins/io.github.pavellizunov.rog-cetra-control/setup
```

After updates, verify an actual visible change. The tested host sometimes kept
old QML despite reload logs. If needed, restart the shell when unlocked and when
interruption of shell services is acceptable. Never launch a second Quickshell
instance for this plugin.

```bash
omarchy plugin disable io.github.pavellizunov.rog-cetra-control
omarchy plugin remove io.github.pavellizunov.rog-cetra-control
```

There is no system service. Generated helpers are removed with the folder; logs
remain. After the owner has stopped, remove the log and `.old` backup described
below if you want to delete diagnostics.

## Security and privacy

- One long-running owner opens receiver interface 3. Other clients use the
  private UNIX socket, not a competing hidraw reader.
- Runtime has no network calls. Repository/package installation and updates use
  the network. Setup does not download or execute Windows firmware tools.
- The helper sends the documented read queries and explicit control reports.
  Call requests and valid session lighting replay can occur automatically; see
  [HANDBOOK.md](HANDBOOK.md) for cadence and [RESEARCH.md](RESEARCH.md) for opcodes.
- Settings and presence/charging expire after 30 seconds. Battery/mode lack a
  general read-response watchdog and may remain historical.
- Runtime socket/lock/cache use `$XDG_RUNTIME_DIR`. The fallback uses fixed names
  under `/tmp` and remains unsafe for multi-user use. Cache replacement uses a
  private unique temporary file and rename; that does not secure the whole path.
- IPC validates command domains and framing. Owner sends handle partial writes,
  disconnecting clients on backpressure. Mirror/stdout backpressure remains open.
- Capture detection reads `pactl` metadata, not audio samples. Hardware serial
  numbers are not collected. No system microphone mute or audio routing is changed.
- Owner-only telemetry logs commands/RGB, gestures, battery/settings, lifecycle,
  timestamps and raw unhandled HID bytes. This can reveal usage timing.
- Log path: `${XDG_STATE_HOME:-$HOME/.local/state}/omarchy/rog-cetra-control.log`.
  If unsafe/unavailable, a verified private runtime directory is tried. Files are
  no-follow, owner/type/single-link checked and mode 0600. Checks do not validate
  every ancestor. Old untouched backups are not retroactively repaired.
- Above 5 MiB the log rotates to one `.old` backup. Logging is synchronous and
  has no runtime opt-out. Shared-shell teardown remains best effort for hung
  descendants; one passing normal exit does not cover every reload race.

## Development and verification

[MODULES.md](MODULES.md) maps changes to source owners and tests.
[HANDBOOK.md](HANDBOOK.md) describes architecture; [CONTRIBUTING.md](CONTRIBUTING.md)
contains contribution rules. Historical records are not current runtime specs.

```bash
./tests/run.sh
omarchy plugin validate .
git diff --check
```

Tests additionally require Python 3, Node.js, a C++ compiler, Qt6Quick/Qt6Qml/Qt6Gui
development libraries and the installed Omarchy shell sources. Some suites still
require `/tmp/opencode` to exist; this is temporary-path debt, not an OpenCode
installation requirement. Tests compile in isolation and never open real HID.
Offline suites and actual Qt checks do not replace live hardware/UI acceptance.

For offline JSON output from the already-built fixture helper:

```bash
CETRA_STATUS_FIXTURE='{"status":"ok","receiver":false,"microphone_state":"unknown"}' ./bin/cetra-status
```

This prints JSON; it does not preview the UI or inject fixtures into a running shell.

## Compatibility and license

Only the SpeedNova USB receiver listed above has been tested. Versions through
1.2.1 used `io.github.pavellizunov.rog-cetra-battery`; remove that old plugin before
installing the renamed one. ROG, Cetra, SpeedNova and ASUS are ASUSTeK trademarks.
This community project is not affiliated with ASUS. Source is MIT; see [LICENSE](LICENSE).
