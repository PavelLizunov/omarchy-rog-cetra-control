"""Real private socket/pipe transport; the held owner lock prevents all HID I/O."""
import fcntl
import os
import pathlib
import socket
import subprocess
import sys
import tempfile
import threading
import time

with tempfile.TemporaryDirectory(prefix="cetra-mirror-", dir="/tmp/opencode") as tmp:
    root = pathlib.Path(tmp)
    with (root / "rog-cetra-control.owner.lock").open("w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        with socket.socket(socket.AF_UNIX) as server:
            server.bind(str(root / "rog-cetra-control.sock"))
            server.listen(1)
            server.settimeout(5)
            proc = subprocess.Popen([sys.argv[1]], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE, env=dict(os.environ, XDG_RUNTIME_DIR=tmp, CETRA_STATUS_FIXTURE=""))
            peer, _ = server.accept()
            payload = b'{"status":"fixture"}\n' * 20000
            commands = b"call off\n" * 20000
            received = bytearray()
            errors = []
            def remote():
                try:
                    with peer:
                        peer.settimeout(5)
                        peer.sendall(payload)
                        while len(received) < len(commands):
                            chunk = peer.recv(8192)
                            if not chunk: break
                            received.extend(chunk)
                except Exception as error:
                    errors.append(error)
            worker = threading.Thread(target=remote)
            worker.start()
            try:
                # Fill the output pipe before the client begins draining it.
                time.sleep(0.2)
                output, stderr = proc.communicate(commands, timeout=10)
            finally:
                if proc.poll() is None: proc.kill(); proc.wait()
                worker.join(timeout=6)
            assert not worker.is_alive() and not errors, errors
            assert output == payload, (len(output), len(payload))
            assert bytes(received) == commands, (len(received), len(commands))
            assert not stderr, stderr
print("PASS mirror: bounded backpressure, ordered bidirectional data, EOF drain, no HID owner")

# Peer closes while stdout is deliberately unread: drain must have a deadline.
with tempfile.TemporaryDirectory(prefix="cetra-mirror-stall-", dir="/tmp/opencode") as tmp:
    root = pathlib.Path(tmp)
    with (root / "rog-cetra-control.owner.lock").open("w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        with socket.socket(socket.AF_UNIX) as server:
            server.bind(str(root / "rog-cetra-control.sock"))
            server.listen(1)
            server.settimeout(5)
            proc = subprocess.Popen([sys.argv[1]], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE, env=dict(os.environ, XDG_RUNTIME_DIR=tmp, CETRA_STATUS_FIXTURE=""))
            peer, _ = server.accept()
            try:
                peer.sendall(b'x' * 16384)
                peer.close()
                proc.stdin.close()
                proc.wait(timeout=4)
            finally:
                if proc.poll() is None: proc.kill(); proc.wait()
print("PASS mirror: stalled output after EOF cannot retain process indefinitely")
