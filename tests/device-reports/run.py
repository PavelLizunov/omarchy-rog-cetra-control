#!/usr/bin/env python3
"""Offline presence/charging JSON contract against an unmodified C snapshot."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import subprocess
import tempfile


HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
PRESENCE = {0: (False, False), 1: (True, False),
            16: (False, True), 17: (True, True)}
CHARGING = {**PRESENCE, 255: (False, False)}
CASE = {0: False, 1: True}


def fixtures():
    commands, expected = [], {}
    presence = charging = None
    stable = {}

    def add(event, now, action="record", value="", observation=None, fields=None):
        nonlocal presence, charging, stable
        if action == "start":
            presence = charging = None
            stable = dict(receiver=False, connected=False, left=37, right=82, case=100,
                          mode="ambient", anc_level=None, anc_adaptive=None,
                          voice_prompt="unknown", proximity=None, lighting="unknown",
                          call_context=value == "call", tap_seq=19)
            if value == "max":
                stable.update(left=None, right=None, case=None, mode="unknown",
                              anc_level=None, proximity=None,
                              tap_seq="INT_MAX")
        if action == "reset":
            presence = charging = None
            stable.update(receiver=False, connected=False, left=None, right=None,
                          case=None, mode="unknown", anc_level=None, anc_adaptive=None,
                          voice_prompt="unknown", proximity=None)
        if observation:
            kind, raw, extra = observation
            if kind == 1:
                presence = (raw, extra, now)
            else:
                charging = (raw, extra, now)
        stable.update(fields or {})
        state = dict(status="ok", microphone_state="unknown", **stable)
        for report, mapping, keys, raw_key in (
            (presence, PRESENCE, ("left_present", "right_present"), "presence_raw"),
            (charging, CHARGING, ("left_charging", "right_charging"), "charging_raw"),
        ):
            fresh = report is not None and 0 <= now - report[2] < 30000
            state.update(zip(keys, mapping.get(report[0], (None, None))
                             if fresh else (None, None)))
            state[raw_key] = report[0] if report else None
        fresh = charging is not None and 0 <= now - charging[2] < 30000
        state["case_charging"] = CASE.get(charging[1]) if fresh else None
        state["case_charging_raw"] = charging[1] if charging else None
        if event in expected:
            raise ValueError(f"duplicate fixture {event}")
        expected[event] = state
        commands.append(f"{event} {now} {action} {value}\n")

    def packet(event, now, kind, raw, extra=0, size=64):
        payload = bytes([0xcc, 0x12, kind, 0, 0, raw, extra] + [0] * 57)[:size]
        observation = (kind, raw, extra) if size >= (6 if kind == 1 else 7) else None
        add(event, now, "packet", payload.hex() or "-", observation)

    add("startup", 0, "start")
    add("unseen_late", 99999)
    for context in ("media", "call"):
        add(f"domain_{context}", 100000, "start", context)
        for raw in range(256):
            packet(f"presence_{context}_{raw}", 100000, 1, raw)
            packet(f"charging_{context}_{raw}", 100000, 8, raw, raw % 2)
        for mask in (0, 17, 2, 255):
            for extra in range(256):
                packet(f"case_{context}_{mask}_{extra}", 100000, 8, mask, extra)

    for kind in (1, 8):
        for size in range(65):
            prefix = f"size_{kind}_{size}"
            add(prefix + "_start", 1000, "start", "call")
            packet(prefix + "_unseen", 1000, kind, 17, 1, size)
            packet(prefix + "_seed_presence", 2000, 1, 1)
            packet(prefix + "_seed_charging", 2000, 8, 16, 0)
            packet(prefix + "_apply", 31999, kind, 17, 1, size)
            add(prefix + "_ttl", 32000)

    for kind in (1, 8):
        prefix = f"ttl_{kind}"
        add(prefix + "_start", 0, "start")
        packet(prefix + "_zero_clock", 0, kind, 17, 1)
        add(prefix + "_29999", 29999)
        add(prefix + "_30000", 30000)
        add(prefix + "_long_stale", 90000)
        packet(prefix + "_refresh", 100000, kind, 0, 0)
        add(prefix + "_backward", 99999)
        add(prefix + "_restored_clock", 100000)
        packet(prefix + "_invalid", 110000, kind, 254, 1)
        add(prefix + "_invalid_29999", 139999)
        packet(prefix + "_short_invalid", 139999, kind, 17, 0, 5 if kind == 1 else 6)
        add(prefix + "_invalid_30000", 140000)

    add("independent_start", 1000, "start", "call")
    packet("independent_presence", 1000, 1, 1)
    packet("independent_charging", 11000, 8, 16, 1)
    add("independent_presence_stale", 31000)
    packet("independent_presence_refresh", 35000, 1, 17)
    add("independent_charging_29999", 40999)
    add("independent_charging_stale", 41000)
    add("independent_both_stale", 65000)

    add("other_start", 1000, "start", "call")
    packet("other_seed_presence", 1000, 1, 17)
    packet("other_seed_charging", 1000, 8, 17, 1)
    others = [
        ("battery", "cc1207000005006464", dict(receiver=True, connected=True, left=0, right=100)),
        ("mode", "cc1225000001", dict(mode="anc")),
        ("consumer", "0c08", {}),
        ("telephony", "0501", {}),
        ("tap", "cc700000000101", dict(tap_seq=20)),
        ("unsolicited_battery", "cc1209000005ffffff", {}),
        ("wrong_report", "cd120100001101", {}),
        ("wrong_family", "cc130800001101", {}),
        ("wrong_command", "cc120200001101", {}),
    ]
    for name, payload, fields in others:
        add("other_" + name, 30999, "packet", payload, fields=fields)
    add("other_no_refresh", 31000)
    add("reset_stale", 31000, "reset")
    packet("reset_new_presence", 32000, 1, 16)
    packet("reset_new_charging", 32000, 8, 255, 1)
    add("reset_fresh", 32001, "reset")
    add("reset_repeated", 32002, "reset")
    packet("after_reset_charging_only", 32003, 8, 1, 0)
    add("restart", 32004, "start")
    packet("after_restart_presence_only", 32005, 1, 0)

    add("capacity_start", 100000, "start", "max")
    packet("capacity_presence", 100000, 1, 0)
    packet("capacity_charging", 100000, 8, 255, 255)
    add("capacity_emit", 100000, "emit")
    add("capacity_stale_emit", 130000, "emit")
    add("capacity_reset", 130001, "reset")
    add("capacity_reset_emit", 130001, "emit")
    return "".join(commands), expected


def inspect(stdout, expected, source):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"duplicate JSON key: {key}")
            result[key] = value
        return result

    rows = [json.loads(line, object_pairs_hook=unique) for line in stdout.splitlines()]
    errors = []
    if [row.get("event") for row in rows] != list(expected):
        errors.append("event stream differs: missing, duplicated, extra or reordered records")
    for row in rows:
        event, state = row.get("event"), row.get("state")
        maximum = row.get("int_max")
        if type(maximum) is not int or maximum < 32767 or not isinstance(state, dict):
            errors.append(f"{event}: invalid harness metadata/state")
            continue
        for key in ("mic_live", "mic_muted"):
            if key in state:
                errors.append(f"{event}: forbidden field {key}")
        for key, value in expected.get(event, {}).items():
            if value == "INT_MAX":
                value = maximum
            elif value == "INT_MIN":
                value = -maximum - 1
            actual = state.get(key)
            if key not in state:
                errors.append(f"{event}: missing field {key}, expected {value!r}")
            elif type(actual) is not type(value) or actual != value:
                errors.append(f"{event}: {key}={actual!r}, expected {value!r}")
    longest = max((row["line_bytes"] for row in rows), default=0)
    # emit_state is exercised dynamically; the uncalled owner has its own cache buffer.
    owner = re.search(r"static int owner\([^\n]*\)\s*\{.*?^}", source, re.M | re.S)
    expanded = source
    for name, number in re.findall(r"^#define\s+(\w+)\s+(\d+)\s*$", source, re.M):
        expanded = re.sub(r"\b" + re.escape(name) + r"\b", number, expanded)
    owner = re.search(r"static int owner\([^\n]*\)\s*\{.*?^}", expanded, re.M | re.S)
    capacity = re.search(r"\bchar\s+last\[(\d+)\]", owner[0]) if owner else None
    if not capacity:
        errors.append("cannot verify owner last[] capacity: inspect updated source declaration")
    elif int(capacity[1]) <= longest:
        errors.append(f"owner last[{capacity[1]}] cannot retain {longest} bytes plus NUL")
    return len(rows), longest, errors


def limits():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    resource.setrlimit(resource.RLIMIT_CPU, (10, 10))
    resource.setrlimit(resource.RLIMIT_FSIZE, (16 * 1024 * 1024, 16 * 1024 * 1024))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sanitize", action="store_true", help="Enable ASan and UBSan")
    parser.add_argument("--revision", help="Read REV:cetra-watch.c using git show, without checkout")
    args = parser.parse_args()
    build = Path(tempfile.mkdtemp(prefix="device-reports-", dir="/tmp/opencode"))
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
        include = build / "hidapi"
        include.mkdir()
        header = (HERE.parent / "lighting-safety/hidapi/hidapi.h").read_bytes()
        (include / "hidapi.h").write_bytes(header)
        executable = build / "device-reports-harness"
        command = ["cc", "-std=gnu11", "-O2", "-Wall", "-Wextra", "-Werror"]
        if args.sanitize:
            command += ["-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                        "-fno-omit-frame-pointer"]
        command += [f"-DCETRA_SOURCE={json.dumps(str(snapshot))}", "-I", str(build),
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
            result["errors"].append("compilation failed; see compile.log (source contract may be pending)")
        else:
            script, expected = fixtures()
            (build / "input.txt").write_text(script)
            run = subprocess.run([str(executable)], input=script.encode(), cwd=build, env=env,
                                 capture_output=True, timeout=20, preexec_fn=limits)
            (build / "events.jsonl").write_bytes(run.stdout)
            (build / "stderr.log").write_bytes(run.stderr)
            result["run_exit"] = run.returncode
            if run.returncode or run.stderr:
                result["errors"].append("harness failed or emitted diagnostics; see stderr.log")
            count, longest, errors = inspect(run.stdout.decode(), expected, source.decode())
            result.update(events=count, expected_events=len(expected), max_line_bytes=longest)
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
