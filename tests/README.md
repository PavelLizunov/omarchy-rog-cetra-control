# Test map

Run `./tests/run.sh`, `omarchy plugin validate .` and `git diff --check` from the
repository root. All must exit zero before completion. Build outputs remain
outside the active plugin; generated helpers in bin/ are not test artifacts.

| Suite | What it exercises |
| --- | --- |
| microphone-state | Unknown mute, gesture lengths/counter, obsolete commands |
| device-reports | Presence/charging domains and TTL; owner scheduling/expiry |
| settings-readback | Settings parser, absence veto, round robin, write/readback |
| ipc-safety | Domains, split frames, partial sends, client removal |
| log-safety | Permissions, links/FIFO rejection, rotation/cache failure |
| lighting-safety | Startup/replay and every transaction failure step |
| call-context | Detection/debounce and isolated timeout process tree |
| lighting-color | RGB/settings, auto-theme, actual Qt colors and label geometry |
| i18n | Catalogs/placeholders, fallback, stale callbacks, plain text |
| service-lifecycle | Shared requests, 48-tick expiry, late replies, retry, keyboard |

## Modular source snapshots

`source_snapshot.py` expands only private `daemon/*.h` includes into frozen test
source without preprocessing or changing logic. Hashes cover the entire native
implementation. Historical revisions read their own headers through Git; old
monolithic revisions still work. Tests never revert the live worktree.

`qml-source.js` enumerates production view/service modules for offline extraction
and source assertions. It does not instantiate the production component tree.
`lighting-color/qt-color.cpp` checks selected real Qt bindings offscreen; it starts
neither another Quickshell nor a HID reader. C harnesses mock/abort hardware APIs.

Dependencies: Python 3, Node.js, C/C++ compilers, pkg-config, hidapi for selftest
compilation, Qt6Quick/Qt6Qml/Qt6Gui development files and installed Omarchy sources.
Current temporary paths require `/tmp/opencode`. Sanitizers are optional suite
flags, not part of every aggregate run.

Record source hash, command, exit and scope. Offline green does not prove rendered
focus, every locale, device tolerance or release readiness. After moving code,
keep transcript/JSON assertions and check actual panel delivery; stale QML after
hot reload was observed on the tested host. See ../MODULES.md for owners.
