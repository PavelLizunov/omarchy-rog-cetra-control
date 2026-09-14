#!/usr/bin/env python3
"""Build only the mocked owner harness; never link hidapi or invoke daemon main."""

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
    resource.setrlimit(resource.RLIMIT_FSIZE, (65536, 65536))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--revision", help="Optional git revision of the source for an isolated RED control")
    parser.add_argument("--summary", action="store_true", help="Print a compact result; full evidence remains in result.json")
    args = parser.parse_args()
    if not args.source.is_absolute() or not args.source.is_file():
        parser.error("--source must be an absolute path to an existing C source")

    build_parent = Path("/tmp/opencode")
    build = Path(tempfile.mkdtemp(prefix="lighting-safety-", dir=build_parent))
    result = {"source": str(args.source), "revision": args.revision,
              "build": str(build), "ok": False, "cases": []}
    try:
        import sys
        sys.path.insert(0, str(HERE.parent))
        from source_snapshot import source_bytes
        source = source_bytes(args.source, args.revision)
        result["source_sha256"] = hashlib.sha256(source).hexdigest()
        snapshot = build / "source.c"
        snapshot.write_bytes(source)
        executable = build / "lighting-harness"
        # Only the agreed field is probed, to let historical controls compile and
        # fail actual behavioral checks rather than fail at the C compiler.
        has_flag = b"lighting_desired_valid" in source
        command = [
            "cc", "-std=gnu11", "-O0", "-g", "-Wall", "-Wextra",
            "-Wno-unused-parameter", "-Wno-unused-variable",
            "-Werror=implicit-function-declaration",
            f"-DCETRA_SOURCE={json.dumps(str(snapshot))}",
            f"-DTEST_HAS_DESIRED_VALID={int(has_flag)}",
            "-I", str(HERE),
            str(HERE / "harness.c"), "-o", str(executable),
        ]
        with (build / "compile.log").open("wb") as log:
            compiled = subprocess.run(command, stdout=log, stderr=log, timeout=30,
                                      env=dict(os.environ, TMPDIR=str(build)))
        result["compile_command"] = command
        result["compile_exit"] = compiled.returncode
        result["compiler_output"] = (build / "compile.log").read_text()[-4000:]
        if compiled.returncode:
            result["error"] = (build / "compile.log").read_text()[-4000:]
        else:
            cases = [(name, 0, "negative") for name in (
                "passive-startup", "passive-case", "passive-usb",
                "static-case", "static-usb", "static-presence-case", "off-case", "off-usb",
                "numeric-handle", "numeric-consume",
            )]
            cases += [(name, step, failure) for name in (
                "first-failure", "replacement-failure",
                "static-case-failure", "static-usb-failure",
            ) for step in range(1, 5) for failure in ("negative", "short")]
            cases += [(name, step, failure) for name in (
                "consume-reconnect", "consume-same-block", "consume-fragment",
                "owner-first-failure", "owner-replacement-failure",
                "call-static-case-failure", "call-static-usb-failure",
            ) for step in range(1, 5) for failure in ("negative", "short")]
            cases += [(name, 4, failure) for name in (
                "ipc-clients", "ipc-stdin", "ipc-clients-fragment",
            ) for failure in ("negative", "short")]
            cases += [(name, 0, "negative") for name in (
                "call-static-case", "call-static-usb",
            )]
            cases += [(name, 0, failure) for name in (
                "call-static-case-call-write-failure", "call-static-usb-call-write-failure",
            ) for failure in ("negative", "short")]
            orders = ("battery-first-case", "presence-first-case", "battery-first-batch-case",
                      "presence-first-batch-case", "presence-only")
            cases += [(f"{intent}-resume-{order}", 0, "negative")
                      for intent in ("passive", "static", "call-passive", "call-static") for order in orders]
            cases += [(f"call-static-resume-{order}-failure", step, failure)
                      for order in orders for step in range(1, 5) for failure in ("negative", "short")]
            cases += [(f"call-static-resume-{order}-call-write-failure", 0, failure)
                      for order in orders for failure in ("negative", "short")]
            # Reuse a successful command's filesystem but exec a fresh process.
            # It must not resurrect desired from the previous owner's cache/log.
            cases += [("new-process", 0, "negative")]
            roots = {}
            for name, step, failure in cases:
                label = f"{name}-{step}-{failure}"
                if name == "new-process":
                    root = roots["static-usb"]
                else:
                    root = build / label
                    root.mkdir(mode=0o700)
                    roots[name] = root
                out = build / f"{label}.stdout"
                err = build / f"{label}.stderr"
                env = dict(os.environ, HOME=str(root), XDG_RUNTIME_DIR=str(root),
                           XDG_STATE_HOME=str(root), TMPDIR=str(root), TZ="UTC0")
                entry = {"name": label, "ok": False}
                try:
                    with out.open("wb") as stdout, err.open("wb") as stderr:
                        run = subprocess.run(
                            [str(executable), name, str(step), failure, str(root)],
                            cwd=root, env=env, stdin=subprocess.DEVNULL,
                            stdout=stdout, stderr=stderr, timeout=10, preexec_fn=limits,
                        )
                    entry["exit"] = run.returncode
                    states = [json.loads(line) for line in out.read_text().splitlines()]
                    diagnostics = err.read_text().splitlines()
                    entry["ok"] = run.returncode == 0
                    if not name.startswith(("first-", "replacement-", "consume-", "numeric-")):
                        initial_unknown = bool(states) and states[0]["lighting"] == "unknown"
                        connected = any(state["connected"] for state in states)
                        entry["ok"] &= initial_unknown and connected
                        if not initial_unknown or not connected:
                            entry["state_error"] = "expected initial unknown lighting and a connected battery state"
                        if "case" in name or "usb" in name:
                            transitions = []
                            for state in states:
                                if not transitions or transitions[-1] != state["connected"]:
                                    transitions.append(state["connected"])
                            if transitions[:4] != [False, True, False, True]:
                                entry["ok"] = False
                                entry["lifecycle_error"] = "missing battery disconnect/reconnect transition"
                        cache = json.loads((root / "rog-cetra-control.status").read_text())
                        entry["ok"] &= bool(states) and cache == states[-1]
                        desired = "off" if name.startswith("off-") else "static" if (
                            "static" in name or name == "owner-replacement-failure" or name.startswith("ipc-")
                        ) else "unknown"
                        timeline = []
                        for line in diagnostics:
                            if line.startswith("STATE "):
                                _, tick, state = line.split(" ", 2)
                                timeline.append((int(tick), json.loads(state)))
                        errors = []
                        if [tick for tick, _ in timeline] != list(range(14)):
                            errors.append("missing bounded owner state timeline")
                        for tick, state in timeline:
                            expected = desired if tick >= 2 else "unknown"
                            if state["lighting"] != expected:
                                errors.append(f"tick {tick}: lighting={state['lighting']}, expected {expected}")
                            call = False
                            if name.startswith("call-"):
                                call = tick >= 2
                            elif name.startswith(("owner-", "ipc-")):
                                call = 2 <= tick < (8 if "fragment" in name else 3)
                            if state["call_context"] != call:
                                errors.append(f"tick {tick}: call_context={state['call_context']}, expected {call}")
                        if cache["lighting"] != desired or cache["call_context"]:
                            errors.append("final cache lost successful desired or retained call context")
                        if errors:
                            entry["ok"] = False
                            entry["timeline_errors"] = errors
                        entry["final_lighting"] = cache["lighting"]
                    entry["detail"] = "\n".join(
                        line for line in diagnostics if not line.startswith("STATE ")
                    )[-1200:]
                except (OSError, ValueError, subprocess.TimeoutExpired) as error:
                    entry["ok"] = False
                    entry["error"] = str(error)
                result["cases"].append(entry)
            result["passed"] = sum(case["ok"] for case in result["cases"])
            result["total"] = len(result["cases"])
            result["ok"] = result["passed"] == result["total"]
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        result["error"] = str(error)
    output = json.dumps(result, indent=2)
    (build / "result.json").write_text(output + "\n")
    if args.summary:
        print(json.dumps({key: result.get(key) for key in
                          ("ok", "passed", "total", "source_sha256", "build", "error")}))
    else:
        print(output)
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
