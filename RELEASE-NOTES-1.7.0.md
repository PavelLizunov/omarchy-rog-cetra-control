# ROG Cetra Control 1.7.0

Release notes prepared for publication; the release has not been published yet.

## Changes

- Optional microphone signal meter beside the bar icon. One audio-only libpulse
  helper reads peaks from the physical Cetra input; recordings and peak history
  are not saved. The setting defaults Off.
- Event-driven PipeWire topology detection replaces periodic audio CLI probes.
  Ordinary recording and communication capture are classified separately;
  processors and keepalives alone do not start the meter or request call context.
- Source pinning keeps the meter on Cetra with the tested EasyEffects setup.
- Bounded owner/mirror output, checked runtime directory and lock file, atomic
  helper installation and explicit unlocked-session guard.
- Thirty-second battery/mode freshness flags, validated percentages and honest
  presence-without-battery display.
- Bounded preferences readback, one-shot meter retries, monotonic peak timing,
  diagnostic opt-out and improved warning-text contrast.

## Install or update

After adding or updating the plugin, run its `setup` script while Omarchy is
running and unlocked. It builds all three local helpers; `libpulse` is now needed.
Full commands, requirements and removal instructions are in README.md.

## Verified scope

Tested with Omarchy 4.0.3, Quickshell 0.3.1, WirePlumber 0.5.17 and PipeWire 1.6.8.
Automated regression and sanitizer suites cover the named cases in tests/README.md.
Live trials covered Discord through EasyEffects, call entry/exit and shell restart,
USB recovery, left/right/case availability, ANC controls, voice prompts, lighting
effects, keyboard use and preference readback. A 30-minute mixed-workload trial
showed no sustained helper RSS/FD growth; it is not proof against all leaks.

## Known limitations

- Native hardware microphone mute remains Unknown. Signal level and silence are
  not mute readback; follow the headset's own voice prompt.
- Continuous capture can suppress native Play/Pause taps on the tested path even
  with no Cetra meter or requested call. The plugin does not alter other services
  or synthesize media keys.
- Suspend/resume, multiple monitors, complete RTL/screen-reader coverage and all
  rendered theme states remain untested. A fresh colored replay retest was deferred.
- Host disable/re-enable can remove inline preferences. Back up the entry first.
- Filesystem I/O is synchronous; pathological storage stalls can delay the owner.
- The preview image is historical. Independent review and fluent-human review of
  every translation were not performed.

Supported hardware: ASUS ROG Cetra True Wireless SpeedNova USB receiver
0b05:1ad3, interface 3. Other Cetra models and Bluetooth are outside tested scope.
