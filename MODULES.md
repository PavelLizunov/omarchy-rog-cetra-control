# Source map

Start with the row matching the change. Read the listed owner and its consumer;
do not load the entire repository by default. Protocol evidence is in RESEARCH.md.

## UI and services

| Change | Owner | Consumer / check |
| --- | --- | --- |
| Bar placement, popup, keyboard traversal | `Cetra.qml` | Installed Omarchy Panel/KeyboardPanel; `tests/service-lifecycle/keyboard.js` |
| View-facing status labels and actions | `CetraViewModel.qml` | All sections; `tests/i18n/run.js`, `tests/lighting-color/run.js` |
| Battery columns | `BatterySection.qml` | View model; device-report and live rendering checks |
| ANC modes, manual levels, Adaptive | `NoiseSection.qml` | View model → service; `tests/service-lifecycle/run.js` |
| Language picker | `LanguageSection.qml` | View model → preferences; `tests/i18n/run.js` |
| Microphone information | `MicrophoneSection.qml` | Unknown mute only; microphone-state contract tests |
| Audio topology and endpoint/call classification | `AudioTopology.qml` | Service, CallDetector, MicrophoneMeter; `tests/audio-topology.js` |
| Optional input peak meter | `MicrophoneMeter.qml` | Service Loader; topology admission; `tests/microphone-meter.js` |
| Bar signal display | `MicrophoneLevel.qml` | Nullable level; no mute inference; `tests/microphone-meter.js` |
| Lighting effects | `LightingSection.qml` | Service's last-sent effect; lighting-color tests |
| RGB inputs, Apply, auto-theme opt-in | `LightingPalette.qml` | View model → service; lighting-color and Qt layout tests |
| Headset voice language | `VoiceSection.qml` | Verified protocol enums; service-lifecycle tests |
| Wrapping button / nullable toggle | `ControlButton.qml`, `SettingToggle.qml` | Explicit `panelRoot` keyboard/theme dependency; Qt and live focus checks |
| SVG recoloring | `CetraIcon.qml` | `assets/`; symbolic white masks, runtime theme color |
| Locale loading and fallback | `I18n.qml`, `locales/index.json` | One instance per view; `tests/i18n/run.js` |
| Persisted UI preferences | `CetraPreferences.qml` | Scoped shell API + watch-only FileView + bounded cetra-status reader |
| Watcher lifecycle, status, pending requests, auto-theme | `CetraService.qml` | Manifest service; service-lifecycle and lighting-color tests |
| Audio communication settlement | `CallDetector.qml` | One child of service; event call-context tests |

Sections receive the explicit `root` view-model reference. They own layout, not
HID or helper processes. Controls receive `panelRoot`, which supplies the common
keyboard target, theme values and focus methods. The entry point inherits the
view model; the service inherits preferences. There is no runtime source stitching.

## Native helper

`cetra-watch.c` compiles as one translation unit. Private implementation headers
under `daemon/` keep static linkage and permit test harnesses to intercept OS/HID
calls before compilation. They are not a public C library or standalone headers.

| Change | Owner | Check |
| --- | --- | --- |
| Owner election, poll schedule, receiver recovery | `cetra-watch.c` | settings-readback owner cases; device-reports owner-ttl; lighting-safety |
| State structures, limits, clock and stop flag | `daemon/types.h` | All C contract suites |
| Verified HID builders and enum names | `daemon/protocol.h` | lighting-safety; IPC domains; RESEARCH.md |
| Incoming reports, expiry and JSON | `daemon/reports.h` | device-reports, settings-readback, microphone-state |
| Command domains and stream framing | `daemon/commands.h` | ipc-safety; lighting-safety |
| Socket fan-out and mirror | `daemon/ipc.h` | ipc-safety; owner fixtures |
| Log permissions, rotation and cache replacement | `daemon/files.h` | log-safety |
| `--selftest` | `daemon/selftest.h` | `tests/run.sh`; no HID access |
| Settings readback and offline fixture CLI | `cetra-status.c` | `--read-settings`, `tests/settings-input.py`, `--selftest`, fixture env |
| Audio-only peak capture, source pinning, EOF teardown | `cetra-peak.c` | `MicrophoneMeter.qml`; `--selftest`, `tests/peak-client.py`, authorized `tests/peak-live.py` |
| Local compilation/deployment | `setup` | Isolated two-run setup check; lock guard |

Include order is explicit in `cetra-watch.c`: types → protocol → files → reports
→ commands → IPC → selftest. The owner loop remains together because its ordering
is part of the verified transcript. Do not split it into independently scheduled
tasks or introduce another hidraw reader.

## Test infrastructure

- `tests/source_snapshot.py` expands only private `daemon/*.h` includes into a
  test snapshot. Historical revisions are read from Git without checkout. Hashes
  cover the expanded implementation, not only the small entry point.
- `tests/qml-source.js` enumerates the production QML files used by offline JS
  assertions. Concatenation is test-only and does not model Qt object ownership.
- `tests/lighting-color/qt-color.cpp` checks actual Qt color passing, settings
  ordering and constrained labels. It does not run another Quickshell instance.
- `tests/run.sh` is the aggregate gate. See `tests/README.md` for limits and commands.

## Documentation ownership

- `README.md`: installation, use, persistence, permissions and user-visible limits.
- `HANDBOOK.md`: architecture and state/ownership contracts.
- `RESEARCH.md`: dated hardware evidence, provenance and unresolved protocol facts.
- `BACKLOG.md`: active work and release blockers.
- `RELEASE.md`: candidate evidence and submission fields; not publication approval.
- `CHANGELOG.md`: changes by release; `REVIEW-2026-09-08.md` and `docs/archive/`
  are historical records, not current instructions or passing-test evidence.

When moving code, update this map, build/snapshot inputs and owning tests in the
same change. File length is a navigation aid, not a reason to split cohesive code.
