# Device Report Contract Tests

Run from the repository root:

```bash
python3 -B tests/device-reports/run.py
python3 -B tests/device-reports/run.py --sanitize
python3 -B tests/device-reports/run.py --revision HEAD
```

The normal run is also invoked by `tests/run.sh`. Requires Python 3 and a C
compiler; the sanitizer variant additionally requires ASan/UBSan. No receiver,
running owner, real HID library, shell restart, or hardware access is required.
`--revision` snapshots source with `git show`, never changes the worktree, and
returns nonzero when that revision fails the contract. A baseline predating the
new JSON fields should compile but fail the independent expected-field checks.

## Isolation

The runner expands `cetra-watch.c` and private `daemon/*.h` through
`tests/source_snapshot.py`, then snapshots the harness and existing
`tests/lighting-safety/hidapi/hidapi.h` mock header in a private mode-0700
`/tmp/opencode/device-reports-*` directory. Build products, inputs, JSON events,
compiler diagnostics, stderr and the complete `result.json` remain there for
inspection. Nothing is built in the repository. The summary records the source
SHA-256 and exit codes. Compiler failures are reported, not skipped or patched
around while the parent source is changing.

The C harness includes that source with its `main` renamed, calls the production
parser/formatter/reset functions, and substitutes a controllable monotonic clock.
All HID entry points abort, including reads, enumeration and writes. Every packet
uses a non-NULL inert device handle so a conditional accidental write cannot hide
behind a NULL guard. Production file opens and sockets are blocked. The explicit
`emit_state` check captures stdout in a private temporary file, makes cache opens
fail with EACCES, supplies no clients, and checks complete output and duplicate
suppression. It never calls the owner or daemon main.

## Contract Coverage

- Independent Python lookup tables cover all 256 presence and charging masks,
  in media and call contexts, and all 256 case bytes for masks 0, 17, 2 and 255.
- Presence accepts only 0, 1, 16 and 17. Charging additionally accepts the HAL
  255 sentinel as explicitly false/false. Other masks decode to null/null.
- Case charging accepts only 0/1, independently of charging-mask validity.
- Every decoded value is a nullable JSON boolean; raw fields are nullable
  integers, including preserved invalid bytes. Missing keys and duplicate JSON
  keys fail. An unseen report has null raw and decoded fields.
- Packet sizes 0 through 64 use exact heap allocations (NULL for zero). Presence
  needs six bytes of `cc 12 01`; charging needs seven of `cc 12 08`. Short reports
  neither create an observation nor overwrite/refresh an existing one.
- Freshness is tested at receipt time zero, 29999 and 30000 ms, long staleness,
  clock rollback and restoration, invalid full-report replacement, short invalid
  packets, separate presence/charging refreshes and unrelated reports.
- Invalid full reports are still observations: their raw bytes replace previous
  bytes, and their independently valid case byte has the new timestamp. Short
  packets, including malformed short packets, do not refresh anything.
- Reset clears both fresh and stale observations; repeated resets, aggregate
  zero-initialized restarts and single-report recovery leave the other unseen.
- Metadata alone preserves battery levels, receiver/connection state, mode,
  settings, call context and tap count. Real battery/mode/tap fixtures retain
  their existing semantics without refreshing metadata. Microphone state must
  always be `unknown`, with no `mic_live` or `mic_muted` fields.
- Formatting uses `char[1024]`, checks NUL/newline termination, and exercises
  production `emit_state` with long false/null values, three-digit raw bytes,
  unknown mode/lighting names and integer extrema. The actual owner's numeric
  `char last[N]` declaration is checked against the largest observed JSON line
  plus NUL; changes to that declaration require reviewing this source guard.

The tests initialize only existing shared state fields and never access
`presence_report`, `charging_report` or a dedicated expiration helper directly.
Freshness may be calculated inline during formatting. ASan exposes boundary
overreads; strict-only runs do not prove memory safety. These checks do not
verify physical hardware semantics, owner polling/lifecycle, UI rendering,
real cache writes, socket delivery, or release readiness.
