# Microphone State Regression Tests

Run from the repository root:

```bash
python3 -B tests/microphone-state/run.py
python3 -B tests/microphone-state/run.py --sanitize
python3 -B tests/microphone-state/run.py --revision HEAD
```

The default build uses `cc -std=gnu11 -O2 -Wall -Wextra -Werror`.
`--sanitize` adds AddressSanitizer and UndefinedBehaviorSanitizer with fatal
diagnostics, retaining the strict optimized build. Python 3, a C compiler and
the requested sanitizer runtimes are the only dependencies; no real HID library
is linked. The declaration-only header is reused through the include path
`tests/lighting-safety/hidapi/hidapi.h`.

The harness includes `cetra-watch.c` and its private `daemon/*.h`, expanded
without logic changes by source_snapshot.py, with
its `main` renamed and never called. It directly exercises `apply_packet`,
`reset_receiver_state`, `consume_commands` and `format_state`, not a rewritten
parser. Every HID function aborts if called, including enumeration, initialization,
close, reads and writes. Packets always use a NULL device; obsolete commands use
both NULL and an inert non-NULL sentinel to detect accidentally guarded writes.
Source-level file opens and socket creation also abort. Logging stays disabled.
No owner loop, daemon executable, setup, live socket, cache, log or hardware is
used. Restart means a new state struct, not a real daemon restart; reconnect means
reset plus battery parser input, not the owner's transport/replay loop.

Each invocation creates a private directory under `/tmp/opencode` and retains
`source.c`, `harness.c`, the test executable, `compile.log`, `events.jsonl`,
`stderr.log` and `result.json`. HOME, XDG paths and TMPDIR point there for subprocesses.
No generated files go into the active plugin tree. The runner prints one summary
JSON object containing exit codes, source SHA-256, event count, errors and the
evidence directory. Exit 0 means all checks passed; exit 1 means a failure.

## Assertions and Events

Every event is serialized using production `format_state` and parsed in Python.
All states must have `microphone_state: "unknown"`, neither `mic_live` nor
`mic_muted`, and the exact expected integer `tap_seq` and boolean `call_context`.
The complete event order and count are checked, not just selected output lines.

| Events | Expected contract |
| --- | --- |
| `startup` | New struct: count 0, media context, no receiver/connection |
| `media_gesture_0..7` | Left/right single, double, triple and long gestures: count 0 |
| `media_consumer`, `telephony_not_readback` | Neither consumer PlayPause nor telephony OffHook is absolute mute readback; count 0 |
| `call_on`, `call_other_gesture_{0,1,2,3,5,6,7}` | Left gestures and right non-single gestures do not count; NULL-device left double is safe |
| `right_call`, `right_duplicate` | Counts 1 then 2, including a byte-identical duplicate |
| `reordered_double`, `reordered_right`, `reordered_left` | Counts 2, 3, 3 for a reordered gesture mixture |
| `missing_report`, `after_missing` | No delivered report means no increment (3); next delivered right single gives 4 |
| `call_off`, `right_media_after_call`, `call_on_again` | Context changes/media tap preserve count 4 |
| `receiver_reset` | Count 4 survives reset; receiver/connection false and batteries null |
| `reconnect_battery`, `missing_battery_1`, `missing_battery_2`, `return_battery` | Count 4 throughout; battery telemetry connects, debounces one missing report, disconnects on two, reconnects |
| `restart_new_struct`, `restart_call_on`, `restart_right_call` | Fresh struct discards old count: 0, 0, 1; no absolute claim |
| `obsolete_{context}_{handle}_before`, `obsolete_{context}_{handle}_chunk_0..4` | Media/call context crossed with NULL/sentinel device; muted, live, batched CRLF/LF and fragmented obsolete commands leave the entire JSON state identical; framing and call request preserved, zero HID calls |
| `short_{fixture}_0..8` | Exact-length allocations for right single, left double, right long, left single. Length 0 uses NULL data. Only right single at lengths 7 and 8 advances seed 7 to 8 |
| `saturation_before`, `saturation_reach`, `saturation_repeat`, `saturation_full` | INT_MAX-1 advances once to INT_MAX; further 7/8-byte singles saturate without signed overflow |

Seven-byte non-single gestures also exercise the optional sub-gesture path:
there is no byte 7 to read. ASan checks the allocation boundary; disabled logging
does not assert the textual `sub=-1` diagnostic. No randomized opcodes or packets
are generated or sent to hardware.

Example `events.jsonl` startup record (one JSON object per line):

```json
{"event":"startup","int_max":2147483647,"state":{"status":"ok","receiver":false,"connected":false,"left":null,"right":null,"case":null,"mode":"unknown","anc_level":null,"anc_adaptive":null,"voice_prompt":"unknown","proximity":null,"lighting":"unknown","call_context":false,"tap_seq":0,"microphone_state":"unknown"}}
```

This excerpt omits presence/charging fields for readability. Tests check full
production records; unrelated settings start unknown until valid readback.

## Historical RED Control

`--revision HEAD` reads the entry point and private headers from Git without touching the active
source. Against the original microphone-inference snapshot it must compile and
fail Python's JSON assertions, starting at `startup`: the old serializer exposes
`mic_live` instead of `microphone_state`. No harness member access depends on
either old or new microphone fields. Once HEAD contains the correction this
command can pass; select the original commit explicitly to retain a RED control.

Historical sanitized runs are deliberately rejected: the old seven-byte path
reads out of bounds and its counter can overflow. The normal historical control
can encounter that old undefined behavior and is not memory-safety evidence;
its meaningful RED evidence is the startup JSON schema violation, before those
boundary cases. Only the current-source sanitizer run verifies those boundaries.
