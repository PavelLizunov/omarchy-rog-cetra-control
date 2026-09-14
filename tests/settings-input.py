"""Fixed-path settings reader: bound bytes before QML buffering, no HID access."""
import os
import pathlib
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix='cetra-config-', dir='/tmp/opencode') as tmp:
    folder = pathlib.Path(tmp) / '.config' / 'omarchy'
    folder.mkdir(parents=True)
    target = folder / 'shell.json'
    env = dict(os.environ, HOME=tmp)
    def run():
        return subprocess.run([sys.argv[1], '--read-settings'], env=env, capture_output=True, timeout=5)
    assert run().returncode == 1
    for payload in (b'', b'{"version":1}', b'x' * 1048576):
        target.write_bytes(payload)
        result = run()
        assert result.returncode == 0 and result.stdout == payload
    target.write_bytes(b'x' * 1048577)
    result = run()
    assert result.returncode == 1 and not result.stdout
    target.unlink()
    os.mkfifo(target)
    assert run().returncode == 1
    target.unlink()
    target.mkdir()
    assert run().returncode == 1
    target.rmdir()
    target.write_text('{"locale":"ru"}')
    # The GUI must not receive data from a failed bounded acquisition.
    source = pathlib.Path(__file__).resolve().parent.parent / 'CetraPreferences.qml'
    qml = source.read_text()
    assert 'preload: false' in qml and 'savedConfig.text(' not in qml and 'savedConfig.reload(' not in qml
    assert 'generation === root.settingsReadGeneration && exitCode === 0' in qml
print('PASS settings reader: fixed path, exact/over byte limit, absent file, FIFO and directory')
