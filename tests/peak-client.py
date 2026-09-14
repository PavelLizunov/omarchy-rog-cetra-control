"""Offline failure contracts. Never connects to the user's audio server."""
import os
import subprocess
import sys
import socket
import tempfile
import time
import pathlib

binary = sys.argv[1]
env = dict(os.environ, PULSE_SERVER="unix:/nonexistent/cetra-peak-test.sock")
for source in ["@DEFAULT_SOURCE@", "alsa_input.laptop", "x" * 4096,
               "alsa_input.usb-ASUSTek_ROG_CETRA_TRUE_WIRELESS_SPEEDNOVA_x.;touch /tmp/x"]:
    result = subprocess.run([binary, source], env=env, capture_output=True, timeout=4)
    assert result.returncode == 2, (source, result)
name = "alsa_input.usb-ASUSTek_ROG_CETRA_TRUE_WIRELESS_SPEEDNOVA_0000-00.mono-fallback"
result = subprocess.run([binary, name], env=env, capture_output=True, timeout=4)
assert result.returncode == 1, result
assert b'"level":0' not in result.stdout, result
print("PASS peak client: source validation, unavailable server, no fabricated zero")

# A silent local protocol peer verifies startup timeout and cleanup without
# reaching PipeWire or creating a capture stream.
with tempfile.TemporaryDirectory(prefix='cetra-pulse-', dir='/tmp/opencode') as tmp:
    address = str(pathlib.Path(tmp) / 'pulse.sock')
    with socket.socket(socket.AF_UNIX) as server:
        server.bind(address)
        server.listen(4)
        env['PULSE_SERVER'] = 'unix:' + address
        proc = subprocess.Popen([binary, name], env=env, stdin=subprocess.PIPE,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            proc.wait(timeout=5)
            assert proc.returncode == 1
            assert all(line == b'{"level":null}' for line in proc.stdout.read().splitlines())
        finally:
            if proc.poll() is None: proc.kill(); proc.wait()
        for action in ('eof', 'term'):
            proc = subprocess.Popen([binary, name], env=env, stdin=subprocess.PIPE,
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            time.sleep(0.1)
            if action == 'eof': proc.stdin.close()
            else: proc.terminate()
            try:
                proc.wait(timeout=2)
                assert proc.returncode == 0, (action, proc.stderr.read())
            finally:
                if proc.poll() is None: proc.kill(); proc.wait()
print('PASS peak lifecycle: silent-server startup deadline, EOF and SIGTERM, no audio capture')

# Reap an orphaned helper ourselves and keep its stdin open, so this checks
# PDEATHSIG rather than accidentally proving ordinary stdin EOF again.
import ctypes
libc = ctypes.CDLL(None, use_errno=True)
assert libc.prctl(36, 1, 0, 0, 0) == 0
with tempfile.TemporaryDirectory(prefix='cetra-parent-', dir='/tmp/opencode') as tmp:
    with socket.socket(socket.AF_UNIX) as server:
        address = str(pathlib.Path(tmp) / 'pulse.sock')
        server.bind(address)
        server.listen(2)
        env['PULSE_SERVER'] = 'unix:' + address
        readfd, writefd = os.pipe()
        parent = subprocess.Popen([sys.executable, '-c',
            'import subprocess,sys,time; p=subprocess.Popen([sys.argv[1],sys.argv[2]],stdin=int(sys.argv[3]),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL); print(p.pid,flush=True); time.sleep(30)',
            binary, name, str(readfd)], env=env, pass_fds=(readfd,), stdout=subprocess.PIPE, text=True)
        child = int(parent.stdout.readline())
        try:
            time.sleep(0.2)
            parent.terminate()
            parent.wait(timeout=2)
            deadline = time.monotonic() + 2
            while True:
                pid, status = os.waitpid(child, os.WNOHANG)
                if pid:
                    assert os.waitstatus_to_exitcode(status) == 0
                    break
                assert time.monotonic() < deadline, 'Peak survived parent death'
                time.sleep(0.02)
        finally:
            if parent.poll() is None: parent.kill(); parent.wait()
            os.close(readfd); os.close(writefd)
            try: os.kill(child, 9); os.waitpid(child, 0)
            except (ProcessLookupError, ChildProcessError): pass
print('PASS peak parent death: child reaped while stdin remained open')
