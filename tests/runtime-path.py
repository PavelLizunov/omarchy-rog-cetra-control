"""Invalid runtime roots must fail before receiver access; never run a valid owner."""
import os
import pathlib
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="cetra-runtime-", dir="/tmp/opencode") as tmp:
    root = pathlib.Path(tmp)
    public = root / "public"
    public.mkdir(mode=0o755)
    for value in [None, "", "relative", str(public)]:
        env = dict(os.environ, CETRA_STATUS_FIXTURE="")
        if value is None:
            env.pop("XDG_RUNTIME_DIR", None)
        else:
            env["XDG_RUNTIME_DIR"] = value
        result = subprocess.run([sys.argv[1]], env=env, capture_output=True, timeout=3)
        assert result.returncode == 1, (value, result)
    assert not list(public.iterdir()), "Invalid root was modified"
    # A directory/FIFO at the lock path must not reach HID initialization.
    for kind in ("directory", "fifo"):
        lock = root / "rog-cetra-control.owner.lock"
        if kind == "directory": lock.mkdir()
        else: os.mkfifo(lock)
        env = dict(os.environ, XDG_RUNTIME_DIR=tmp, CETRA_STATUS_FIXTURE="")
        result = subprocess.run([sys.argv[1]], env=env, capture_output=True, timeout=3)
        assert result.returncode == 1
        if kind == "directory": lock.rmdir()
        else: lock.unlink()
print("PASS runtime: missing/relative/shared roots and invalid lock objects rejected before HID")
