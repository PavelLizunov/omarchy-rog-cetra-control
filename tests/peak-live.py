"""Authorized live audio-only trial: own one meter, inspect routes, then close it.

Usage: python3 -B tests/peak-live.py /path/to/cetra-peak
Requires a single connected Cetra and an already-active external capture client.
No PCM or individual level samples are persisted. Existing streams are read-only.
"""
import json
import selectors
import subprocess
import sys
import time


def outputs():
    return json.loads(subprocess.check_output(
        ["pactl", "-f", "json", "list", "source-outputs"], timeout=3))


def own(stream):
    return stream["properties"].get("application.id") == "io.github.pavellizunov.rog-cetra-control.peak"


sources = json.loads(subprocess.check_output(["pactl", "-f", "json", "list", "sources"], timeout=3))
sources = [s for s in sources if s["name"].startswith(
    "alsa_input.usb-ASUSTek_ROG_CETRA_TRUE_WIRELESS_SPEEDNOVA_")]
assert len(sources) == 1
source = sources[0]
before = outputs()
assert not any(own(s) for s in before), "Run before enabling the service meter"
routes = {s["index"]: s["source"] for s in before}
proc = subprocess.Popen([sys.argv[1], source["name"]], stdin=subprocess.PIPE,
                        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
poller = selectors.DefaultSelector()
poller.register(proc.stdout, selectors.EVENT_READ)
frames = numeric = 0
low, high = 1.0, 0.0
try:
    end = time.monotonic() + 8
    while time.monotonic() < end and proc.poll() is None:
        for key, _ in poller.select(0.2):
            line = key.fileobj.readline()
            if not line:
                break
            value = json.loads(line)["level"]
            frames += 1
            if value is not None:
                assert 0 <= value <= 1
                numeric += 1
                low, high = min(low, value), max(high, value)
    during = outputs()
    meters = [s for s in during if own(s)]
    assert proc.poll() is None, proc.stderr.read()
    assert len(meters) == 1 and meters[0]["source"] == source["index"], meters
    props = meters[0]["properties"]
    assert props.get("node.dont-move") == "true", props
    assert props.get("node.passive") == "in-follow", props
    assert numeric > 20, (frames, numeric)
    after_routes = {s["index"]: s["source"] for s in during if not own(s)}
    assert after_routes == routes, (routes, after_routes)
    print(f"PASS live peaks: {numeric}/{frames} numeric frames; range {low:.3f}–{high:.3f}; unchanged external routes")
finally:
    proc.stdin.close()
    try:
        proc.wait(timeout=4)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()
        raise
    poller.close()
assert proc.returncode == 0, proc.stderr.read()
assert not any(own(s) for s in outputs())
print("PASS EOF teardown: helper exited 0 and its capture stream disappeared")
