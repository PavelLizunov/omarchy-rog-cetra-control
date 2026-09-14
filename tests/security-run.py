"""Shared isolated build runner for the two security harnesses."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import subprocess
import tempfile


def run(here):
    parser = argparse.ArgumentParser()
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--source", type=Path)
    args = parser.parse_args()
    repo = here.parent.parent
    import sys
    sys.path.insert(0, str(here.parent))
    from source_snapshot import source_bytes
    source = source_bytes(args.source or repo / "cetra-watch.c")
    build = Path(tempfile.mkdtemp(prefix=here.name + "-", dir="/tmp/opencode"))
    snapshot = build / "source.c"
    snapshot.write_bytes(source)
    env = dict(os.environ, HOME=str(build), XDG_RUNTIME_DIR=str(build),
               XDG_STATE_HOME=str(build), TMPDIR=str(build), CETRA_STATUS_FIXTURE="",
               ASAN_OPTIONS="detect_leaks=1:halt_on_error=1:disable_coredump=1",
               UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    command = ["cc", "-std=gnu11", "-O2", "-Wall", "-Wextra", "-Werror",
               f"-DCETRA_SOURCE={json.dumps(str(snapshot))}",
               "-I", str(here.parent / "lighting-safety"), str(here / "harness.c"),
               "-o", str(build / "harness")]
    if args.sanitize:
        command += ["-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                    "-fno-omit-frame-pointer"]
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    result = dict(ok=False, build=str(build), source_sha256=hashlib.sha256(source).hexdigest(),
                  sanitize=args.sanitize)
    try:
        compiled = subprocess.run(command, env=env, capture_output=True, timeout=30)
        (build / "compile.log").write_bytes(compiled.stdout + compiled.stderr)
        assert compiled.returncode == 0, compiled.stderr.decode()
        tested = subprocess.run([str(build / "harness")], cwd=build, env=env,
                                capture_output=True, timeout=20)
        (build / "stdout").write_bytes(tested.stdout)
        (build / "stderr").write_bytes(tested.stderr)
        assert tested.returncode == 0, tested.stderr.decode()
        result.update(ok=True, detail=tested.stdout.decode().splitlines()[-1])
    except (AssertionError, OSError, subprocess.SubprocessError) as error:
        result["error"] = str(error)
    (build / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))
    return 0 if result["ok"] else 1
