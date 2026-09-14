# Lighting Safety Regressions

Run from the repository root:

```sh
python3 -B tests/lighting-safety/run.py --source /absolute/path/to/cetra-watch.c
```

An optional `--revision dadeb305` reads the historical source with `git show`
without changing the worktree. Exit 0 means all cases passed; exit 1 means RED
or a build/execution error. Stdout is one JSON object with per-case results,
the tested source SHA-256, exact compiler command/output and the artifact
directory. The same JSON is saved there as `result.json`.

The runner expands private `daemon/*.h` through `tests/source_snapshot.py` and
hashes the complete implementation. `harness.c` includes that snapshot via
`CETRA_SOURCE`, renames but never calls its
original `main`, and runs the actual `owner(-1)` or owner with a fake server FD.
All HID API functions are
local mocks, using the test-local header in
`hidapi/hidapi.h`.
No real hidapi library is linked. Socket creation/connect/bind/listen and
device-open APIs abort. Accept/recv/send/close only recognize two controlled
fake client FDs; no socket is created. Poll, stdin and time are deterministic
mocks, and every failed HID transport rejects subsequent reads or writes until
close. Client readiness, input counts and clean shutdown are asserted.
Filesystem paths use private HOME/XDG directories, with guarded `fopen`.
This is a harness for the reviewed owner code, not a sandbox for hostile C.
Each foreground child is time-, CPU-, and output-bounded, with core dumps
disabled. Snapshots, binaries, cache/log files and captured output are confined
to a unique `/tmp/opencode/lighting-safety-*` directory outside the plugin tree.

Coverage (181 cases in the current runner, not independent reviewer runs):

- Startup battery, case out/back and USB read error/reopen without a command:
  zero lighting output; initial JSON lighting is `unknown`.
- Explicit Static with distinctive RGB and explicit Off: exactly four complete
  reports, plus the same four bytes-for-byte on case and USB restoration.
- Fresh process reusing the previous successful owner's cache/log directory:
  no inherited desired state and no implicit lighting writes.
- First command and replacement: negative and short writes at each of four
  steps; stop at the failed prefix, preserve validity/enum/RGB before and after
  receiver reset (16 direct command cases).
- Case/USB restoration: negative and short writes at each step must close the
  failed receiver before another HID read and restore the retained desired on
  its next open, rather than report success after a partial sequence.
- `consume_commands`: successful Static then failed Breathing, with `call off`
  after reconnect, in the failed block, or split as `call o` / `ff\n` across
  reconnect. Assert false block result, framing length/bytes/overflow, cleared
  `call_requested`, retained desired and no later writes (24 cases).
- Actual owner first/replacement failure: every step, negative and short writes,
  later lighting and call-off commands in the same stdin block, disconnect and
  reopen, no first desired activation and restoration of old Static (16 cases).
- Actual owner IPC: first client or stdin fails lighting while a second client
  is ready in the same poll. The second client's hardware command is suppressed
  but call off is consumed, including a fragmented tail after reconnect. Checks
  both source ordering and NULL-device context synchronization (6 cases).
- Active call-context case/USB restoration: successful call write before
  lighting, lighting failures at all four steps, and negative/short call-write
  failure preventing any lighting on the failed transport (22 cases).
- Every owner case checks a 14-tick cache JSON timeline and final cache/stdout
  agreement. Lighting becomes Static/Off after successful explicit commands,
  remains the last successful desired across failed replacement and reset, and
  stays unknown only when no successful desired exists. Call-context timelines
  catch lost call off even when owner shutdown would later clear the flag.

The historical field probe only permits baseline compilation. It does not
replace a production helper or rewrite the source. Assertions refer only to
existing owner/command entry points and the agreed desired state fields.
Run GREEN against current source. `--revision dadeb305` is a historical RED
control; verify that revision exists first. Modular revisions load their own
headers from Git, never today's files.
The repository's `tests/run.sh` also runs this suite with `--summary`; full JSON
evidence remains in the reported artifact directory. Existing unrelated runtime
issues and real hardware behavior are outside this suite's scope.
