#!/usr/bin/env python3
"""Exercise the actual owner with mock HID/I/O and a deterministic TTL clock."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile


HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
FIELDS = ("left_present", "right_present", "left_charging", "right_charging", "case_charging")
RAW = ("presence_raw", "charging_raw", "case_charging_raw")


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def verify_run(executable, build, name):
    root = build / name
    root.mkdir(mode=0o700)
    with (root / "stdout.jsonl").open("wb") as out, (root / "stderr.log").open("wb") as err:
        run = subprocess.run([str(executable), str(root), name], cwd=root,
                             env=dict(os.environ, HOME=str(root), XDG_RUNTIME_DIR=str(root),
                                      XDG_STATE_HOME=str(root), TMPDIR=str(root)),
                             stdin=subprocess.DEVNULL, stdout=out, stderr=err, timeout=10)
    check(run.returncode == 0, f"{name}: mock owner failed; see {root}/stderr.log")
    states = [json.loads(line) for line in (root / "stdout.jsonl").read_text().splitlines()]
    cache, clients, queries, opens, closes = {}, {1001: [], 1002: []}, [], [], []
    for line in (root / "stderr.log").read_text().splitlines():
        if line.startswith("CACHE "):
            _, tick, state = line.split(" ", 2)
            check(int(tick) not in cache, "duplicate cache checkpoint")
            cache[int(tick)] = json.loads(state)
        elif line.startswith("CLIENT "):
            _, tick, fd, state = line.split(" ", 3)
            clients[int(fd)].append((int(tick), json.loads(state)))
        elif line.startswith("QUERY "):
            _, tick, time, opcode = line.split()
            queries.append((int(tick), int(time), int(opcode, 16)))
        elif line.startswith("OPEN "):
            opens.append(int(line.split()[1]))
        elif line.startswith("CLOSE "):
            closes.append(int(line.split()[1]))
        else:
            raise AssertionError(f"unexpected harness diagnostic: {line}")
    initial_queries = [(1, 0, 7), (1, 0, 0x25), (1, 0, 1)]
    if name == "ttl":
        expected = [(2, 1000, 7), (3, 30999, 0x25), (3, 30999, 1),
                    (6, 40998, 7), (7, 40999, 1)]
        stop = 9
        expected_opens, expected_closes = [0], [41001]
    elif name == "schedule":
        expected = [(3, 500, 7), (5, 1000, 0x25), (6, 9999, 7), (7, 10000, 1),
                    (9, 19999, 0x25), (10, 20000, 1), (11, 55000, 7), (11, 55000, 1),
                    (13, 64999, 0x25), (14, 65000, 1)]
        stop = 15
        expected_opens, expected_closes = [0], [65001]
    elif name.startswith("startup-"):
        expected = [(3, 1000, 7), (3, 1000, 0x25), (3, 1000, 1),
                    (5, 10999, 7), (6, 11000, 1)]
        stop = 7
        expected_opens, expected_closes = [0, 1000], [0, 11001]
    else:
        expected = [(2, 9999, 7), (3, 10000, 1), (6, 11000, 7), (6, 11000, 0x25),
                    (6, 11000, 1), (8, 20999, 7), (9, 21000, 1)]
        stop = 10
        expected_opens, expected_closes = [0, 11000], [10000, 21001]
    check(queries == initial_queries + expected, f"{name}: wrong query transcript: {queries}")
    check(opens == expected_opens and closes == expected_closes, f"{name}: wrong reconnect/backoff")
    check(list(cache) == list(range(stop)), "missing owner iteration checkpoints")
    final = json.loads((root / "rog-cetra-control.status").read_text())
    check(all(cache[0][key] is None for key in FIELDS + RAW), "initial reports must be null")
    check(final == cache[0], "shutdown cache did not reset reports")
    if name == "ttl":
        fresh = cache[2]
        check([fresh[key] for key in FIELDS] == [True, True, True, False, True],
              "incorrect presence11/charging01/case1 decoding at fake1000")
        check([fresh[key] for key in RAW] == [17, 1, 1], "missing raw report bytes")
        check(cache[3] == fresh, "reports expired before fake31000")
        expired = dict(fresh, **dict.fromkeys(FIELDS))
        check(all(cache[tick] == expired for tick in (4, 5, 6)),
              "unanswered queries must not prevent TTL expiry or emit idle changes")
        recovered = dict(expired, left_present=False, right_present=True, presence_raw=16)
        check(cache[7] == recovered and cache[8] == recovered,
              "next query response must restore presence only, without idle duplicates")
        partial = dict(fresh, **dict.fromkeys(FIELDS[2:] + RAW[1:]))
        check(states == [cache[0], cache[1], partial, fresh, expired, recovered, final],
              "stdout missed a transition or emitted an idle duplicate")
        check(cache[1] == dict(cache[0], receiver=True), "unexpected initial receiver state")
        check(clients[1001] == list(zip([1, 1, 2, 2, 4, 7, 9], states)),
              "client1 missed transition or received idle duplicate")
        check(clients[1002] in ([(4, expired), (7, recovered), (9, final)],
                               [(4, fresh), (4, expired), (7, recovered), (9, final)]),
              "client2 missed expiry/recovery")
    else:
        check(all(all(state[key] is None for key in FIELDS + RAW) for state in states),
              "query without a response fabricated observed state")
        online = dict(cache[0], receiver=True)
        expected_states = [cache[0], online, final]
        if name.startswith("periodic-"):
            expected_states = [cache[0], online, cache[0], online, final]
        check(states == expected_states, "idle query changed emission count or disconnect state")
    for state in states:
        check(state["microphone_state"] == "unknown" and state["tap_seq"] == 0 and
              state["call_context"] is False and state["lighting"] == "unknown" and
              state["connected"] is False, "reports changed unrelated device state")
    return {"name": name, "stdout_states": len(states), "queries": queries,
            "cache_checkpoints": len(cache), "run_exit": run.returncode}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=REPO / "cetra-watch.c")
    parser.add_argument("--negative-control", action="store_true",
                        help="Delete only the outer loop's final emit in a private snapshot; expect exit 1")
    args = parser.parse_args()
    build = Path(tempfile.mkdtemp(prefix="owner-ttl-", dir="/tmp/opencode"))
    result = {"ok": False, "build": str(build), "negative_control": args.negative_control}
    try:
        import sys
        sys.path.insert(0, str(HERE.parent))
        from source_snapshot import source_bytes
        source = source_bytes(args.source).decode()
        result["source_sha256"] = hashlib.sha256(source.encode()).hexdigest()
        if args.negative_control:
            anchor = "    emit_state(&state, clients, last, sizeof(last));\n  }\n\n  if (device) {"
            check(source.count(anchor) == 1, "outer final emit anchor is not unique")
            source = source.replace(anchor, "  }\n\n  if (device) {", 1)
        snapshot = build / "source.c"
        snapshot.write_text(source)
        executable = build / "owner-ttl"
        command = ["cc", "-std=gnu11", "-O0", "-g", "-Wall", "-Wextra",
                   "-Wno-unused-variable", "-Werror=implicit-function-declaration",
                   f"-DCETRA_SOURCE={json.dumps(str(snapshot))}",
                   "-I", str(HERE.parent / "lighting-safety"),
                   str(HERE / "owner-ttl.c"), "-o", str(executable)]
        result["compile_command"] = command
        with (build / "compile.log").open("wb") as log:
            compiled = subprocess.run(command, stdout=log, stderr=log, timeout=30,
                                      env=dict(os.environ, TMPDIR=str(build)))
        result["compile_exit"] = compiled.returncode
        check(compiled.returncode == 0, "compilation failed; see compile.log")
        result["cases"] = [verify_run(executable, build, name) for name in
                           ("ttl", "schedule", "startup-negative", "startup-short",
                            "periodic-negative", "periodic-short")]
        result["ok"] = True
    except (AssertionError, OSError, ValueError, subprocess.SubprocessError) as error:
        result["error"] = str(error)
    (build / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
