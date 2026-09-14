# Lighting color and preferences

Run `node tests/lighting-color/run.js` from the repository root.

Production owners: CetraViewModel, LightingSection/LightingPalette,
CetraPreferences and CetraService. `tests/qml-source.js` lists their inputs.
Theme/RGB selection saves settings only; Apply/effect actions send validated RGB.
Auto-theme is a separate opt-in and requires an explicit colored apply in the
current session. One timer coalesces updates, deduplicates payloads and preserves
Off/Cycle. Helper/receiver reset disarms it.

Node fixtures execute actual functions with scoped host/transport mocks: all
channels 0–255, invalid inputs, two views, stale snapshots, persistence, effects
and auto-update guards. The runner also compiles Qt6Quick/Qt6Qml/Qt6Gui checks for
QColor passing, host-setting order and 420 constrained production-label layouts.

No hardware is opened, no real preferences are written and no Quickshell process
is started. Label geometry is not full-panel or screen-reader acceptance. Native
packet ordering/failure remains in lighting-safety. See ../README.md for limits.
