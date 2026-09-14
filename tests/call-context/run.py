"""Offline QML function tests. Node.js is a test-only dependency; no QML shell starts."""
import pathlib
import shutil
import subprocess
import sys

here = pathlib.Path(__file__).resolve().parent
node = shutil.which("node")
if node is None:
    sys.exit("Call-context tests require Node.js (test-only dependency)")
logic = subprocess.run(
    [node, str(here / "run.js"), str(here.parent.parent / "CetraService.qml"),
     str(here.parent.parent / "Cetra.qml")],
    check=False, timeout=30,
).returncode
sys.exit(logic)
