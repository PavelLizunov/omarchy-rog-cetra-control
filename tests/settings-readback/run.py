#!/usr/bin/env python3
"""Strict offline readback oracle. No hidapi linkage or production daemon main."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
KEYS = ("anc_level", "anc_adaptive", "voice_prompt", "proximity")
QUERIES = (43, 44, 40, 38)
DOMAINS = ({1: 1, 2: 2, 3: 3}, {0: False, 1: True},
           {0: "sound", 1: "english", 2: "chinese"}, {0: False, 1: True})
UNKNOWN = dict(zip(KEYS, (None, None, "unknown", None)))


def parser_fixtures():
    lines, expected = [], []
    observed = {}
    absent = False

    def event(now, action="record", payload=""):
        nonlocal observed, absent
        if action in ("start", "reset"):
            observed = {}
            absent = False
        if action == "packet":
            data = bytes.fromhex(payload) if payload != "-" else b""
            if len(data) >= 6 and data[:2] == b"\xcc\x12":
                if data[2] == 1:
                    absent = data[5] == 0
                    if absent:
                        observed.clear()
                if data[2] in QUERIES:
                    index = QUERIES.index(data[2])
                    observed.pop(index, None)
                    if data[5] in DOMAINS[index] and not absent:
                        observed[index] = (DOMAINS[index][data[5]], now)
        state = UNKNOWN.copy()
        for index, (value, received) in observed.items():
            if 0 <= now - received < 30000:
                state[KEYS[index]] = value
        expected.append(state)
        lines.append(f"event{len(lines)} {now} {action} {payload}\n")

    def packet(now, opcode, value, size=64, family=0x12):
        data = bytes([0xcc, family, opcode, 0, 0, value] + [0] * 58)[:size]
        event(now, "packet", data.hex() or "-")

    event(0, "start")
    event(90000)
    for index, opcode in enumerate(QUERIES):
        for raw in range(256):
            event(0, "start")
            for other, value in zip(QUERIES, (3, 1, 1, 1)):
                packet(0, other, value)
            packet(1000, opcode, raw)
            event(30000)
            event(31000)
        for size in range(65):
            event(0, "start")
            packet(0, opcode, 1, size)
            packet(1000, opcode, 1)
            packet(30999, opcode, 0xff, size)
            event(31000)
            event(0, "start")
            packet(1000, opcode, 1)
            packet(30999, opcode, 1, size)
            event(31000)
        event(0, "start")
        packet(0, opcode, 1)
        event(29999)
        event(30000)
        packet(40000, opcode, 1)
        event(39999)
        event(40000)
        packet(69999, opcode, 1, family=0x13)
        event(70000)
        packet(80000, opcode, 1)
        event(80001, "reset")
    event(0, "start")
    for index, opcode in enumerate(QUERIES):
        packet(index * 10000, opcode, 1)
    for now in (30000, 39999, 40000, 49999, 50000, 59999, 60000):
        event(now, "emit")
    event(0, "start")
    for opcode in QUERIES:
        packet(0, opcode, 1)
    packet(1000, 1, 0)
    for opcode in QUERIES:
        packet(2000, opcode, 1)
    event(40000)
    packet(41000, 1, 17)
    event(41001)
    for opcode in QUERIES:
        packet(42000, opcode, 1)
    return "".join(lines), expected


def check_settings(actual, expected):
    for key, value in expected.items():
        assert key in actual and type(actual[key]) is type(value) and actual[key] == value, (key, actual, expected)
    assert actual["microphone_state"] == "unknown"


def owner_oracle(stderr, scenario, phase, field):
    records = [line.split(" ", 2) for line in stderr.splitlines() if len(line.split(" ", 2)) == 3]
    actual_queries = [(int(tick), int(value)) for kind, tick, value in records if kind == "QUERY"]
    opens = [int(line.split()[1]) for line in stderr.splitlines() if line.startswith("OPEN ")]
    # Independent event-time model: exact startup, unchanged 07/25 and presence,
    # one optional query per 2500ms, never catch up or invent a response.
    expected_queries, expected_states = [], {}
    connected = polling = absent = live = False
    next_open, next_regular, next_presence, next_settings, query_phase, setting_phase = 1, 0, 0, 0, 0, 0
    observed, failed, generation, missing = {}, False, 0, 0
    expected_opens = []

    def query(tick, opcode, immediate=False):
        nonlocal live, connected, polling, absent, observed, next_open, failed
        expected_queries.append((tick, opcode))
        if not failed and opcode == QUERIES[field] and (scenario == "query" or (scenario == "readback" and immediate)):
            failed = True
            live = connected = polling = absent = False
            observed = {}
            next_open = tick + 4

    for tick in range(1, 181):
        if tick == 10 and scenario in ("command", "write", "readback"):
            assert live
            if scenario == "write":
                live = connected = polling = absent = False
                observed = {}
                next_open = tick + 4
            else:
                observed.pop(field, None)
                query(tick, QUERIES[field], True)
        if not live and tick >= next_open:
            live = True
            generation += 1
            expected_opens.append(tick)
            expected_queries.extend((tick, opcode) for opcode in (7, 37, 1))
            next_regular, next_presence, query_phase = tick + 2, tick + 40, 0
            polling = False
        if live and tick >= next_regular:
            query(tick, (7, 37)[query_phase])
            query_phase ^= 1
            next_regular = tick + 2
        if live and tick >= next_presence:
            query(tick, 1)
            next_presence = tick + 40
        if live:
            if scenario == "usb" and tick == 30:
                live = connected = polling = absent = False
                observed = {}
                next_open = tick + 4
            else:
                if tick == phase or (generation > 1 and tick == phase + 40) or (scenario == "absence" and tick in (26, 148)) or (scenario == "battery" and tick in (25, 26, 30)):
                    if scenario == "battery" and tick in (25, 26):
                        missing += 1
                        if missing == 2:
                            connected = polling = False
                            observed = {}
                    else:
                        connected = True
                        missing = 0
                if scenario == "absence" and tick in (25, 149):
                    absent = tick == 25
                    if absent:
                        polling = False
                        observed = {}
                if tick == phase + 1 or (scenario == "absence" and tick == 27):
                    if not absent:
                        observed = {i: (value, tick) for i, value in enumerate((2, True, "chinese", True))}
                if scenario == "command" and tick == 11:
                    observed[field] = ((2, True, "chinese", True)[field], tick)
        if live and connected and not absent and tick < 180:
            if not polling:
                setting_phase = 0
                polling = True
            if tick >= next_settings:
                next_settings = tick + 10
                query(tick, QUERIES[setting_phase])
                setting_phase = (setting_phase + 1) % 4
        else:
            polling = False
        state = UNKNOWN.copy()
        for index, (value, received) in observed.items():
            if tick - received < 120:
                state[KEYS[index]] = value
        if tick < 180:
            expected_states[tick] = state
    assert actual_queries == expected_queries, (scenario, actual_queries, expected_queries)
    assert opens == expected_opens, (opens, expected_opens)
    caches = {int(tick): json.loads(value) for kind, tick, value in records if kind == "CACHE"}
    assert set(caches) == set(range(180))
    check_settings(caches[0], UNKNOWN)
    for tick, expected in expected_states.items():
        check_settings(caches[tick], expected)
    clients = [json.loads(value) for kind, tick, value in records if kind == "CLIENT"]
    # Every cache transition, including expiry without reports, must reach IPC.
    for tick in range(1, 180):
        if caches[tick] != caches[tick - 1]:
            assert caches[tick] in clients, ("cache change missing from IPC", tick)
    writes = [(int(tick), int(value)) for kind, tick, value in records if kind == "WRITE"]
    assert writes == ([(10, (12, 13, 10, 9)[field])] if scenario in ("command", "write", "readback") else [])
    return len(expected_queries)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    build = Path(tempfile.mkdtemp(prefix="settings-readback-", dir="/tmp/opencode"))
    import sys
    sys.path.insert(0, str(HERE.parent))
    from source_snapshot import source_bytes
    source = source_bytes(REPO / "cetra-watch.c")
    (build / "source.c").write_bytes(source)
    env = dict(os.environ, HOME=str(build), XDG_RUNTIME_DIR=str(build), XDG_STATE_HOME=str(build), TMPDIR=str(build),
               ASAN_OPTIONS="detect_leaks=1:halt_on_error=1:disable_coredump=1", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    result = dict(ok=False, source_sha256=hashlib.sha256(source).hexdigest(), build=str(build), sanitize=args.sanitize)
    try:
        for name, harness in (("parser", HERE.parent / "device-reports/harness.c"), ("owner", HERE / "owner.c")):
            command = ["cc", "-std=gnu11", "-O2", "-Wall", "-Wextra", "-Werror",
                       f"-DCETRA_SOURCE={json.dumps(str(build / 'source.c'))}", "-I", str(HERE.parent / "lighting-safety"), str(harness), "-o", str(build / name)]
            if args.sanitize:
                command += ["-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer"]
            compiled = subprocess.run(command, env=env, capture_output=True, timeout=30)
            (build / f"{name}-compile.log").write_bytes(compiled.stdout + compiled.stderr)
            assert compiled.returncode == 0, compiled.stderr.decode()
        script, expected = parser_fixtures()
        run = subprocess.run([str(build / "parser")], input=script, text=True, cwd=build, env=env, capture_output=True, timeout=30)
        (build / "parser.jsonl").write_text(run.stdout)
        assert run.returncode == 0 and not run.stderr, run.stderr
        rows = [json.loads(line) for line in run.stdout.splitlines()]
        assert len(rows) == len(expected)
        for index, (row, state) in enumerate(zip(rows, expected)):
            assert row["event"] == f"event{index}"
            check_settings(row["state"], state)
        result["parser_events"] = len(rows)
        cases = [("schedule", phase, 0, 0) for phase in range(1, 5)]
        cases += [("schedule", 181, 0, 0)]
        cases += [(name, 2, 0, 0) for name in ("absence", "battery", "usb")]
        cases += [("command", 2, field, 0) for field in range(4)]
        cases += [(name, 2, field, failure) for name in ("query", "write", "readback") for field in range(4) for failure in range(2)]
        total_queries = 0
        for case in cases:
            label = "-".join(map(str, case))
            root = build / label
            root.mkdir(mode=0o700)
            run = subprocess.run([str(build / "owner"), str(root), *map(str, case)], cwd=root, env=env, text=True, capture_output=True, timeout=10)
            (build / f"{label}.stderr").write_text(run.stderr)
            (build / f"{label}.stdout").write_text(run.stdout)
            assert run.returncode == 0, (label, run.stderr[-2000:])
            total_queries += owner_oracle(run.stderr, *case[:3])
            states = [json.loads(line) for line in run.stdout.splitlines()]
            check_settings(states[0], UNKNOWN)
            check_settings(states[-1], UNKNOWN)
            assert json.loads((root / "rog-cetra-control.status").read_text()) == states[-1]
        result.update(ok=True, owner_cases=len(cases), checked_queries=total_queries)
    except (AssertionError, OSError, ValueError, subprocess.SubprocessError) as error:
        result["error"] = str(error)
    (build / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
