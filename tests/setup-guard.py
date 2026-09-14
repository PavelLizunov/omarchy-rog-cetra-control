"""Exercise the actual setup guard only; never install or replace binaries."""
import pathlib
import re
import subprocess

source = (pathlib.Path(__file__).resolve().parent.parent / "setup").read_text()
guard = re.search(r"^ensure_shell_unlocked\(\) \{.*?^\}", source, re.M | re.S)[0]
for payload, expected in [('','blocked'), ('{}','blocked'), ('bad','blocked'),
                         ('{"locked":false,"requested":false,"secure":false}','allowed'),
                         ('{"locked":true,"requested":false,"secure":false}','blocked'),
                         ('{"locked":false,"requested":true,"secure":false}','blocked')]:
    script = 'omarchy-shell() { printf "%s" "$PAYLOAD"; }; timeout() { shift; "$@"; };\n' + guard + '\nensure_shell_unlocked'
    import os
    result = subprocess.run(['bash','-c',script],env=dict(os.environ,PAYLOAD=payload),capture_output=True,timeout=5)
    assert (result.returncode == 0) == (expected == 'allowed'), (payload,result)
print('PASS setup guard: explicit unlocked only, unknown/empty/malformed/locked rejected')
