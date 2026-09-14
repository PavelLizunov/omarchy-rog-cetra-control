"""Run the exact QML detector command against fake audio tools, in isolation."""
import ctypes
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time


HERE = Path(__file__).resolve().parent
source = (HERE.parent.parent / "CallDetector.qml").read_text()
block = re.search(r"^  Process \{\n    id: callContextProc\b.*?^  \}", source, re.M | re.S)[0]
command = json.loads(re.search(r"^    command: (.+)$", block, re.M)[1])
# Adopt and reap test grandchildren after timeout kills the pipeline shell.
if ctypes.CDLL(None, use_errno=True).prctl(36, 1, 0, 0, 0) != 0:
    raise OSError(ctypes.get_errno(), "PR_SET_CHILD_SUBREAPER failed")

mock = """#!PYTHON
import os, signal, sys, time
from pathlib import Path
name = Path(sys.argv[0]).name
root = Path(os.environ['TEST_ROOT'])
signal.signal(signal.SIGTERM, signal.SIG_IGN if os.environ['TEST_IGNORE_TERM'] == '1' else signal.SIG_DFL)
(root / (name + '.json')).write_text(__import__('json').dumps([os.getpid(), os.getpgrp(), os.getppid()]))
while True:
    time.sleep(0.05)
""".replace("PYTHON", sys.executable, 1)


def run_case(ignore_term, external_alarm):
    with tempfile.TemporaryDirectory(prefix="cetra-call-tree-", dir="/tmp/opencode") as directory:
        root = Path(directory)
        # This PATH contains no real pactl or jq, even if one mock fails to start.
        for name in ("pactl", "jq"):
            path = root / name
            path.write_text(mock)
            path.chmod(0o700)
        for name in ("timeout", "bash"):
            shutil.copy2(shutil.which(name), root / name)
        env = dict(os.environ, PATH=str(root), TEST_ROOT=str(root),
                   TEST_IGNORE_TERM=str(int(ignore_term)))
        for key in ("BASH_ENV", "ENV", "SHELLOPTS", "BASHOPTS"):
            env.pop(key, None)
        env = {key: value for key, value in env.items() if not key.startswith("BASH_FUNC_")}
        start = time.monotonic()
        proc = subprocess.Popen(command, cwd=root, env=env, start_new_session=True,
                                stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                stderr=subprocess.DEVNULL)
        children = []
        try:
            deadline = start + 1.5
            while not all((root / (name + ".json")).exists() for name in ("pactl", "jq")):
                if time.monotonic() >= deadline:
                    raise AssertionError("mock pipeline did not start")
                time.sleep(0.01)
            children = [json.loads((root / (name + ".json")).read_text()) for name in ("pactl", "jq")]
            # No --foreground: timeout, bash and both tools share the owned group.
            assert all(group == proc.pid for pid, group, parent in children), children
            parents = {parent for pid, group, parent in children}
            assert len(parents) == 1 and proc.pid not in parents, children
            if external_alarm:
                proc.send_signal(signal.SIGALRM)
            proc.wait(timeout=0.5 if external_alarm else 5)
            # Without --foreground, GNU timeout also kills itself with SIGKILL.
            assert proc.returncode == -signal.SIGKILL, proc.returncode
            assert time.monotonic() - start < 5
            if external_alarm:
                assert time.monotonic() - start < 2, "ALRM did not preempt the normal deadline"
            deadline = time.monotonic() + 1
            while True:
                try:
                    pid, _ = os.waitpid(-1, os.WNOHANG)
                except ChildProcessError:
                    break
                if pid == 0:
                    if time.monotonic() >= deadline:
                        raise AssertionError("pipeline descendants survived supervisor exit")
                    time.sleep(0.01)
            for pid in parents | {pid for pid, group, parent in children}:
                assert not Path(f"/proc/{pid}").exists(), f"pipeline process {pid} survived"
            try:
                os.killpg(proc.pid, 0)
            except ProcessLookupError:
                pass
            else:
                raise AssertionError("process group survived supervisor exit")
            print(f"PASS process tree: ignore_TERM={ignore_term}, external_ALRM={external_alarm}, "
                  f"exit={proc.returncode}, group gone before cleanup")
        finally:
            # Signal only the isolated group created by this test, never host processes.
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            proc.wait(timeout=2)
            deadline = time.monotonic() + 2
            while time.monotonic() < deadline:
                try:
                    pid, _ = os.waitpid(-1, os.WNOHANG)
                except ChildProcessError:
                    break
                if pid == 0:
                    time.sleep(0.01)


for ignore_term in (False, True):
    for external_alarm in (False, True):
        run_case(ignore_term, external_alarm)
