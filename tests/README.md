# Test map

Run `./tests/run.sh`, `omarchy plugin validate .` and `git diff --check` from the
repository root. All must exit zero before completion. Build outputs remain
outside the active plugin; generated helpers in bin/ are not test artifacts.

| Suite | What it exercises |
| --- | --- |
| microphone-state | Unknown mute, gesture lengths/counter, obsolete commands |
| microphone-meter.js | Exact source, active-link/self-monitor gate, null/zero, call exclusion, presence without battery |
| audio-topology.js | Active direct/processed routes, mixed/foreign inputs, endpoint exclusion, graph limits and call roles |
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

Dependencies: Python 3, Node.js, C/C++ compilers, pkg-config, hidapi and libpulse for selftest
compilation, Qt6Quick/Qt6Qml/Qt6Gui development files and installed Omarchy sources.
Current temporary paths require `/tmp/opencode`. Sanitizers are optional suite
flags, not part of every aggregate run.

Record source hash, command, exit and scope. Offline green does not prove rendered
focus, every locale, device tolerance or release readiness. After moving code,
keep transcript/JSON assertions and check actual panel delivery; stale QML after
hot reload was observed on the tested host. See ../MODULES.md for owners.

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
