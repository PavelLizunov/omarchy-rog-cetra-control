#!/usr/bin/env python3
"""Check the active instruction set, user-guide links and aggregate test surface."""
from pathlib import Path
import os
import re
import sys
from urllib.parse import unquote

root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]
instructions = (
    "DEVELOPMENT.md", "CONTRIBUTING.md", "tests/DEVELOPMENT.md",
    "assets/DEVELOPMENT.md", "bin/DEVELOPMENT.md",
)
for name in instructions:
    path = root / name
    assert path.is_file() and not path.is_symlink(), name
# Workspace entries may exist locally, but marketplace archives must not ship them.
if (root / ".git").exists():
    import subprocess
    shipped = subprocess.check_output(["git", "-C", str(root), "ls-files", "-z"]).decode().split("\0")
else:
    shipped = [str(path.relative_to(root)) for path in root.rglob("*") if path.is_file()]
assert not any(Path(name).name in ("AGENTS.md", "CLAUDE.md") for name in shipped), "Do not ship automatically loaded agent instructions (marketplace #6873)"
development = (root / "DEVELOPMENT.md").read_text()
for name in instructions:
    assert name in development, f"Instruction owner must list {name}"
assert not re.search(r"gemini-swarm|opencode-gemini|ninitux/", development)
assert "Dated reviews" in development and "not instructions" in development
assert "workspace-only" in development and "#6873" in development
assert not re.search(r"On \d{4}-\d{2}-\d{2} the user authorized", development)
readme = (root / "README.md").read_text()
assert len(readme.splitlines()) <= 150, "Move README detail to the linked user guide"
assert re.search(r"\[[^]]+\]\(https://github\.com/[^)]+/releases\)", "\n".join(readme.splitlines()[:12]))
assert "docs/user-guide.md" in readme
assert (root / "LICENSE").is_file() and "[MIT](LICENSE)" in readme

for name in (*instructions, "README.md", "docs/user-guide.md"):
    path = root / name
    text = path.read_text()
    for target in re.findall(r"\]\(([^)]+)\)", text):
        if re.match(r"[A-Za-z][A-Za-z0-9+.-]*:", target):
            continue
        filename, _, anchor = target.partition("#")
        destination = (path.parent / unquote(filename)).resolve() if filename else path
        assert destination.is_file(), f"Broken link: {name}: {target}"
        if anchor and destination.suffix == ".md":
            headings = re.findall(r"^#{1,6}\s+(.+)$", destination.read_text(), re.M)
            slugs = {re.sub(r"[^\w\- ]", "", heading.lower()).replace(" ", "-") for heading in headings}
            assert unquote(anchor) in slugs, f"Broken anchor: {name}: {target}"

runner = (root / "tests/run.sh").read_text()
for name in ("tests/documentation.py", "tests/panel-ui/run.py"):
    assert f'python3 -B "$plugin_dir/{name}"' in runner, f"Aggregate omits {name}"
assert (root / "tests/panel-ui/run.py").is_file()
assert '"$plugin_dir/docs/user-guide.md"' in runner, "Keep the moved guide in the native-mute safety scan"
assert "CETRA_PANEL_UI" not in runner, "Panel coverage must not be optional"
assert re.search(r'^python3 -B "\$plugin_dir/tests/panel-ui/run.py"$', runner, re.M), "Run panel checks unconditionally"
panel_runner = (root / "tests/panel-ui/run.py").read_text()
assert "host_panel.index('  onOpenChanged:')" in panel_runner, "Anchor host geometry on code, not prose"
assert "popout coordination" not in panel_runner, "Host comment headings must not delimit test input"
assert "-nolisten" in panel_runner and "server.wait(timeout=5)" in panel_runner
assert "50_mesa.json" in panel_runner and "LIBGL_ALWAYS_SOFTWARE" in panel_runner
assert "GraphicsInfo.OpenGL" in (root / "tests/panel-ui/tst_panel.qml").read_text()
assert "`cetra-bt-read`" in (root / "bin/DEVELOPMENT.md").read_text()
for directory, dirs, files in os.walk(root):
    dirs[:] = [name for name in dirs if name != ".git"]
    for name in (*dirs, *files):
        assert not (Path(directory) / name).is_symlink(), f"Symlink: {directory}/{name}"
print("Documentation, instruction ownership and aggregate surface passed.")
