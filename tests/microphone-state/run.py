#!/usr/bin/env python3
"""Offline microphone contract checks against the actual C source, without HID I/O."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import subprocess
import tempfile


HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent


def limits():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    resource.setrlimit(resource.RLIMIT_CPU, (5, 5))
    resource.setrlimit(resource.RLIMIT_FSIZE, (1024 * 1024, 1024 * 1024))


def inspect(stdout):
    rows = [json.loads(line) for line in stdout.splitlines()]
    expected = {}

    def add(event, tap_seq, call_context, **fields):
        expected[event] = dict(tap_seq=tap_seq, call_context=call_context, **fields)

    add("startup", 0, False, receiver=False, connected=False)
    for i in range(8):
        add(f"media_gesture_{i}", 0, False)
    for event in ("media_consumer", "telephony_not_readback"):
        add(event, 0, False)
    add("call_on", 0, True)
    for i in range(8):
        if i != 4:
            add(f"call_other_gesture_{i}", 0, True)
    for event, count in (("right_call", 1), ("right_duplicate", 2),
                         ("reordered_double", 2), ("reordered_right", 3),
                         ("reordered_left", 3), ("missing_report", 3),
                         ("after_missing", 4)):
        add(event, count, True)
    add("call_off", 4, False)
    add("right_media_after_call", 4, False)
    add("call_on_again", 4, True)
    add("receiver_reset", 4, True, receiver=False, connected=False,
        left=None, right=None, case=None)
    for event in ("reconnect_battery", "missing_battery_1"):
        add(event, 4, True, receiver=True, connected=True, left=91, right=98, case=100)
    add("missing_battery_2", 4, True, receiver=True, connected=False,
        left=None, right=None, case=100)
    add("return_battery", 4, True, receiver=True, connected=True, left=91, right=98, case=100)
    add("restart_new_struct", 0, False, receiver=False, connected=False)
    add("restart_call_on", 0, True)
    add("restart_right_call", 1, True)
    for context in range(2):
        for handle in range(2):
            prefix = f"obsolete_{context}_{handle}"
            add(f"{prefix}_before", 11 + context, bool(context))
            for chunk in range(5):
                add(f"{prefix}_chunk_{chunk}", 11 + context, bool(context))
    for fixture in range(4):
        for size in range(9):
            add(f"short_{fixture}_{size}", 8 if fixture == 0 and size >= 7 else 7, True)
    if not rows or type(rows[0].get("int_max")) is not int or rows[0]["int_max"] < 32767:
        raise ValueError("missing C INT_MAX metadata")
    maximum = rows[0]["int_max"]
    add("saturation_before", maximum - 1, True)
    for event in ("saturation_reach", "saturation_repeat", "saturation_full"):
        add(event, maximum, True)

    errors = []
    if [row.get("event") for row in rows] != list(expected):
        errors.append("event stream differs: missing, duplicated, extra or reordered test records")
    states = {}
    for row in rows:
        event = row.get("event")
        state = row.get("state")
        if not isinstance(state, dict):
            errors.append(f"{event}: state is not an object")
            continue
        states[event] = state
        if row.get("int_max") != maximum:
            errors.append(f"{event}: inconsistent INT_MAX")
        for key in ("mic_live", "mic_muted"):
            if key in state:
                errors.append(f"{event}: forbidden field {key}")
        fields = dict(status="ok", microphone_state="unknown", **expected.get(event, {}))
        for key, value in fields.items():
            actual = state.get(key)
            if key not in state or type(actual) is not type(value) or actual != value:
                errors.append(f"{event}: {key}={actual!r}, expected {value!r}")
        if "_chunk_" in event:
            before = event.split("_chunk_")[0] + "_before"
            if state != states.get(before):
                errors.append(f"{event}: obsolete command changed serialized state")
    return rows, errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sanitize", action="store_true", help="Add ASan and UBSan")
    parser.add_argument("--revision", help="Use git show REV:cetra-watch.c as an isolated RED control")
    args = parser.parse_args()
    if args.sanitize and args.revision:
        parser.error("historical snapshots are strict-only: known seven-byte OOB/overflow")

    build = Path(tempfile.mkdtemp(prefix="microphone-state-", dir="/tmp/opencode"))
    result = dict(ok=False, build=str(build), revision=args.revision,
                  sanitize=args.sanitize, errors=[])
    try:
        import sys
        sys.path.insert(0, str(HERE.parent))
        from source_snapshot import source_bytes
        source = source_bytes(REPO / "cetra-watch.c", args.revision)
        result["source_sha256"] = hashlib.sha256(source).hexdigest()
        snapshot = build / "source.c"
        snapshot.write_bytes(source)
        harness = build / "harness.c"
        harness.write_bytes((HERE / "harness.c").read_bytes())
        executable = build / "microphone-harness"
        command = ["cc", "-std=gnu11", "-O2", "-Wall", "-Wextra", "-Werror"]
        if args.sanitize:
            command += ["-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                        "-fno-omit-frame-pointer"]
        command += [f"-DCETRA_SOURCE={json.dumps(str(snapshot))}",
                    "-I", str(HERE.parent / "lighting-safety"),
                    str(harness), "-o", str(executable)]
        result["compile_command"] = command
        env = dict(os.environ, HOME=str(build), XDG_RUNTIME_DIR=str(build),
                   XDG_STATE_HOME=str(build), TMPDIR=str(build), PYTHONDONTWRITEBYTECODE="1",
                   ASAN_OPTIONS="detect_leaks=1:halt_on_error=1:disable_coredump=1",
                   UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
        compiled = subprocess.run(command, cwd=build, env=env, capture_output=True, timeout=30)
        (build / "compile.log").write_bytes(compiled.stdout + compiled.stderr)
        result["compile_exit"] = compiled.returncode
        if compiled.returncode:
            result["errors"].append("compilation failed; see compile.log")
        else:
            run = subprocess.run([str(executable)], cwd=build, env=env,
                                 stdin=subprocess.DEVNULL, capture_output=True,
                                 timeout=10, preexec_fn=limits)
            (build / "events.jsonl").write_bytes(run.stdout)
            (build / "stderr.log").write_bytes(run.stderr)
            result["run_exit"] = run.returncode
            if run.returncode or run.stderr:
                result["errors"].append("harness failed or emitted diagnostics; see stderr.log")
            rows, errors = inspect(run.stdout.decode())
            result["events"] = len(rows)
            result["errors"].extend(errors)
        result["ok"] = not result["errors"]
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        result["errors"].append(str(error))
    (build / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    summary = {key: value for key, value in result.items() if key != "compile_command"}
    summary["errors"] = result["errors"][:8]
    summary["error_count"] = len(result["errors"])
    print(json.dumps(summary))
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
