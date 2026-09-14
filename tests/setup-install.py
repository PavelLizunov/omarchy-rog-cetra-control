"""Build an isolated source checkout twice; never deploy to the active plugin."""
import os
import pathlib
import shutil
import subprocess
import tempfile

repo = pathlib.Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix="cetra-setup-", dir="/tmp/opencode") as tmp:
    root = pathlib.Path(tmp)
    candidate = root / "candidate with spaces"
    candidate.mkdir()
    for name in ("setup", "cetra-status.c", "cetra-watch.c", "cetra-peak.c", "manifest.json"):
        shutil.copy2(repo / name, candidate / name)
    shutil.copytree(repo / "daemon", candidate / "daemon")
    tools = root / "tools"
    tools.mkdir()
    # Only host lifecycle/installation commands are stubbed. Compilation is real.
    for name, body in {
        "omarchy-shell": '#!/bin/sh\nprintf \'%s\\n\' \'{"locked":false,"requested":false,"secure":false}\'\n',
        "omarchy": '#!/bin/sh\ntest "$1" = plugin && test "$2" = validate\n',
    }.items():
        path = tools / name
        path.write_text(body)
        path.chmod(0o755)
    env = dict(os.environ, PATH=str(tools) + ":" + os.environ["PATH"], XDG_RUNTIME_DIR=tmp)
    def setup():
        return subprocess.run([str(candidate / "setup")], env=env, capture_output=True, timeout=30)
    first = setup()
    assert first.returncode == 0, first.stderr
    def identity():
        return {p.name: (p.stat().st_ino, p.stat().st_mtime_ns, p.read_bytes())
                for p in (candidate / "bin").iterdir()}
    before = identity()
    second = setup()
    assert second.returncode == 0, second.stderr
    assert identity() == before, "Unchanged binaries were replaced"
    # A compile failure must retain every existing binary and leave no temp files.
    (candidate / "cetra-peak.c").write_text("#error expected isolated failure\n")
    failed = setup()
    assert failed.returncode != 0
    assert identity() == before
    assert not list(root.glob("rog-cetra-build.*"))
    assert not list((candidate / "bin").glob(".cetra-install.*"))
print("PASS setup: real builds, spaced paths, unchanged inode/mtime on repeat, compile failure preserves helpers")
