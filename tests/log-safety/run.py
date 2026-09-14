#!/usr/bin/env python3
from pathlib import Path
import runpy

HERE = Path(__file__).resolve().parent
raise SystemExit(runpy.run_path(str(HERE.parent / "security-run.py"))["run"](HERE))
