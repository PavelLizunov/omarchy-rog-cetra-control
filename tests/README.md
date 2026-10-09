# Test map

Run `./tests/run.sh`, `omarchy plugin validate .` and `git diff --check` from the
repository root. All must exit zero before completion. Build outputs remain
outside the active plugin; generated helpers in bin/ are not test artifacts.

| Suite | What it exercises |
| --- | --- |
| documentation.py | Active instruction ownership, local documentation links/anchors, concise README and aggregate panel-test inclusion |
| microphone-state | Unknown mute, gesture lengths/counter, obsolete commands |
| microphone-meter.js | Exact source, active-link/self-monitor gate, null/zero, call exclusion, presence without battery, initial admission/cooldown and Sound signal labels |
| microphone-meter/run.py | Production meter in isolated Qt with fake Process: immediate start, real two-second timer, rapid capture toggles, source replacement, zero, freshness expiry and stop; no recording |
| bar-indicator/run.py | Actual Qt bar rendering: independent earbud charge, unknown/zero, mic no-data/zero/response/visibility, fixed geometry, theme scaling and consumer action wiring; no live shell |
| audio-topology.js | Active direct/processed routes, mixed/foreign inputs, endpoint exclusion, graph limits and call roles |
| bluetooth.js / bluetooth/run.py | Identity/battery/generation and visibility contracts; complete production Qt observer bindings with controlled native-model fixtures, no hardware operations |
| contrast.js | Production warning-color guard, dark/light fallback for insufficient contrast |
| setup-guard.py / runtime-path.py | Explicit unlocked setup admission, private runtime roots and invalid lock objects |
| setup-install.py | Real isolated compilation, repeat-install inode preservation, compilation failure rollback |
| settings-input.py | Fixed-path settings acquisition, exact/over 1 MiB, missing/FIFO/directory rejection |
| qml-lint.py | All QML; isolated qs imports, fatal semantic warnings, declaration-checked host QtObject members |
| peak-client.py + cetra-peak --selftest | Offline source/peak validation and absent-server failure |
| device-reports | Presence/charging domains and TTL; owner scheduling/expiry |
| settings-readback | Settings parser, absence veto, round robin, write/readback |
| ipc-safety | Domains, split frames, partial sends, client removal |
| log-safety | Permissions, links/FIFO rejection, rotation/cache failure |
| lighting-safety | Startup/replay and every transaction failure step |
| call-context | Event-driven detection/debounce, bounded loss settlement and restart intent |
| lighting-color | RGB/settings, auto-theme, actual Qt colors and label geometry |
| i18n | Catalogs/placeholders, fallback, stale callbacks, plain text |
| service-lifecycle | Shared requests, 48-tick expiry, late replies, retry, Sound/Device keyboard focus and hotplug |
| panel-ui/run.py | Full production panel in isolated Qt/OpenGL: mouse/key navigation, scoped errors, save failures, Escape/focus, locale fallback and disabled Apply |

## Modular source snapshots

`source_snapshot.py` expands only private `daemon/*.h` includes into frozen test
source without preprocessing or changing logic. Hashes cover the entire native
implementation. Historical revisions read their own headers through Git; old
monolithic revisions still work. Tests never revert the live worktree.

`qml-source.js` enumerates production view/service modules for offline extraction
and source assertions. It does not instantiate the production component tree.
`lighting-color/qt-color.cpp` checks selected real Qt bindings offscreen; it starts
neither another Quickshell nor a HID reader. C harnesses mock/abort hardware APIs.

Dependencies: Python 3, Node.js, C/C++ compilers, pkg-config, hidapi and libpulse for selftest
compilation, Qt6Quick/Qt6Qml/Qt6Gui development files and installed Omarchy sources.
Current temporary paths require `/tmp/opencode`. Sanitizers are optional suite
flags, not part of every aggregate run.

Record source hash, command, exit and scope. Offline green does not prove rendered
focus, every locale, device tolerance or release readiness. After moving code,
keep transcript/JSON assertions and check actual panel delivery; stale QML after
hot reload was observed on the tested host. See ../MODULES.md for owners.

The aggregate always runs the panel regression. It requires Qt Quick Test
(`qmltestrunner`), Mesa software OpenGL and Xvfb (`xorg-server-xvfb`). The runner
owns a private virtual display, disables TCP listening, selects Mesa GLX/EGL,
and stops/reaps Xvfb after success, failure or timeout. It never connects to the
production display. `CETRA_TEST_XVFB` may name a verified local Xvfb executable.
Qt's offscreen platform still needs GLX here; a missing display previously caused
SIGABRT. Software OpenGL is real OpenGL, not the Qt software scene-graph fallback;
the fixture asserts `GraphicsInfo.OpenGL`. To run it alone, use
`python3 -B tests/panel-ui/run.py`.
Set `CETRA_PANEL_EVIDENCE_DIR` to retain the fixture and PNGs for inspection.
It copies the production panel, view model, sections and installed host controls;
only test object names are added to view files. Services, window ownership and
theme/catalog I/O use inert adapters. It writes no desktop settings and opens
no hardware. Captures use the host's default theme. This does not verify live
Wayland focus, outside-click dismissal, screen placement or device delivery.
The window adapter imports the installed host's fitting and card-origin formulas
with simulated screen/bar inputs. Transition assertions compare requested and
fitted card dimensions, origin, navigation and viewport bounds with a one-unit
tolerance. They cover all captured states, constrained 320 × 420 screens, enlarged
text and fixed wheel dimensions. This is host-formula/fixture evidence, not live
compositor placement evidence.

QML lint policy: unqualified access to explicit lexical section properties is
style advice (`info`). Host Style/Bar QtObject members are accepted only when
their declarations exist in the installed owning source. Quickshell 0.3.1's
unexported QProcess::ExitStatus is an explicit exception for the watcher/settings
exit handlers, which do not use that enum. New semantic diagnostics fail; this is a scoped lint policy,
not a claim of zero raw upstream diagnostics.

`python3 -B tests/peak-live.py /path/to/cetra-peak` is a separate, authorized live
trial, never part of run.sh. Run with the plugin meter disabled and an external
client already using Cetra. It creates one peak stream for eight seconds, checks
the physical source and unchanged external routes, then closes stdin and checks
teardown. External streams appearing/disappearing during the trial invalidate
its route comparison. Individual samples and PCM are not saved.

## Bluetooth vendor telemetry

The aggregate gate runs `bluetooth-telemetry-native.py`,
`bluetooth-telemetry.js` and `bluetooth-telemetry/run.py`. Native checks compile
with libbluetooth and use AF_UNIX socketpairs, never arbitrary Bluetooth targets.
They exercise the production parser/formatter, verified getter packets, nullable
percentages including right91/null/93, charging masks, fragmentation, EOF, frame
limits, CLI validation and busy-reader exclusion. The Qt fixture instantiates the
complete production QML component with a controlled C++ Process adapter matching
Quickshell 0.3.1 exit/failed-start ordering; it never starts a second shell.
JS checks supplement it with wall-clock/cooldown/generation cases and bounded
shared-view membership. Native Qt tooling including moc is a test dependency.
Physical case freshness, power consumption, suspend/resume and multimonitor
acceptance are separate from these offline checks.

## Bar antialiasing

`CETRA_BAR_BACKEND=opengl python3 -B tests/bar-indicator/run.py` explicitly selects
the RHI scene graph and OpenGL, then verifies the actual graphics API. It fails
if Qt falls back to another backend. Requires a working local OpenGL context.
The ordinary aggregate uses software rendering and does not prove accelerated
edge quality. Both exercise a real 16 px instance alongside the enlarged cells.
`CETRA_BAR_EVIDENCE_DIR=/tmp/cetra-bar-evidence` retains the fixture image, actual
size crop and unfiltered pixel zoom for inspection.
