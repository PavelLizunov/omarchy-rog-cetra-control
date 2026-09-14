"""Expand private local C includes for isolated tests and historical controls.

No preprocessing, code rewriting or hardware access. A snapshot contains the
same implementation units as the compiler, so hashes cover every source unit.
"""
from pathlib import Path
import re
import subprocess


def source_bytes(path, revision=None):
    path = Path(path).resolve()
    root = path.parent

    def read(relative, stack=()):
        if relative in stack or relative.startswith('/') or '..' in Path(relative).parts:
            raise ValueError('Invalid local include: ' + relative)
        data = subprocess.run(['git', 'show', f'{revision}:./{relative}'], cwd=root,
                              check=True, capture_output=True, timeout=10).stdout if revision else (root / relative).read_bytes()
        return re.sub(rb'^#include "(daemon/[a-z]+\.h)"\s*$',
                      lambda m: read(m[1].decode(), stack + (relative,)), data, flags=re.M)

    return read(path.name)
