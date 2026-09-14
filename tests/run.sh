#!/usr/bin/env bash

set -euo pipefail

plugin_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$(mktemp -d /tmp/opencode/slovn-cetra-test.XXXXXX)"
trap 'rm -rf "$build_dir"' EXIT
export TMPDIR="$build_dir"

python3 -m json.tool "$plugin_dir/manifest.json" >/dev/null
node "$plugin_dir/tests/module-contract.js"
node "$plugin_dir/tests/microphone-meter.js"
node "$plugin_dir/tests/audio-topology.js"
node "$plugin_dir/tests/contrast.js"
python3 -B "$plugin_dir/tests/qml-lint.py"
test -z "$(find "$plugin_dir" -type l)"

omarchy plugin validate "$plugin_dir"

cc -O2 -Wall -Wextra -Werror \
  -o "$build_dir/cetra-status" \
  "$plugin_dir/cetra-status.c" \
  $(pkg-config --cflags --libs hidapi-hidraw)
cc -O2 -Wall -Wextra -Werror \
  -o "$build_dir/cetra-watch" \
  "$plugin_dir/cetra-watch.c" \
  $(pkg-config --cflags --libs hidapi-hidraw)

test "$("$build_dir/cetra-status" --selftest)" = "ok"
python3 -B "$plugin_dir/tests/settings-input.py" "$build_dir/cetra-status"
test "$("$build_dir/cetra-watch" --selftest)" = "ok"
python3 -B "$plugin_dir/tests/runtime-path.py" "$build_dir/cetra-watch"
python3 -B "$plugin_dir/tests/mirror-transport.py" "$build_dir/cetra-watch"
cc -O2 -Wall -Wextra -Werror -o "$build_dir/cetra-peak" "$plugin_dir/cetra-peak.c" $(pkg-config --cflags --libs libpulse) -lm
test "$("$build_dir/cetra-peak" --selftest)" = "ok"
python3 -B "$plugin_dir/tests/peak-client.py" "$build_dir/cetra-peak"
python3 -B "$plugin_dir/tests/setup-guard.py"
python3 -B "$plugin_dir/tests/setup-install.py"
python3 -B "$plugin_dir/tests/microphone-state/run.py"
python3 -B "$plugin_dir/tests/device-reports/run.py"
python3 -B "$plugin_dir/tests/device-reports/owner-ttl.py"
python3 -B "$plugin_dir/tests/settings-readback/run.py"
python3 -B "$plugin_dir/tests/ipc-safety/run.py"
python3 -B "$plugin_dir/tests/log-safety/run.py"
python3 -B "$plugin_dir/tests/lighting-safety/run.py" --source "$plugin_dir/cetra-watch.c" --summary
grep -Fq 'Omarchy issue #9441' "$plugin_dir/setup"
grep -Fq 'signal(SIGPIPE, SIG_IGN)' "$plugin_dir/cetra-watch.c"
if grep -Eq '0x33|05 33|mic (live|muted)' "$plugin_dir/cetra-watch.c" "$plugin_dir"/daemon/*.h "$plugin_dir/Cetra.qml" "$plugin_dir/CetraService.qml"; then
  printf '%s\n' "Unsupported software microphone state command found" >&2
  exit 1
fi
if grep -Eq 'mic_muted|0xcc, 0x70|proven hardware toggle' "$plugin_dir/cetra-watch.c" "$plugin_dir"/daemon/*.h "$plugin_dir/Cetra.qml" "$plugin_dir/CetraService.qml" "$plugin_dir/README.md"; then
  printf '%s\n' "Unreliable microphone state inference found" >&2
  exit 1
fi

if grep -Eq 'micLive|mic_live|mic_state|cetra-mic-off|"In case"|Earbuds are in the case' "$plugin_dir/Cetra.qml" "$plugin_dir/CetraService.qml"; then
  printf '%s\n' "UI exposes inferred mute or unsupported in-case state" >&2
  exit 1
fi
grep -Eq 'root\.tr\("microphone\.unknown", "[^"]*[Uu]nknown"\)' "$plugin_dir/MicrophoneSection.qml"
grep -Fq 'text: root.levelText(modelData.value)' "$plugin_dir/BatterySection.qml"
python3 -B "$plugin_dir/tests/call-context/run.py"
node "$plugin_dir/tests/lighting-color/run.js"
node "$plugin_dir/tests/i18n/run.js"
node "$plugin_dir/tests/service-lifecycle/run.js"

# Run only the guard function, never the setup/deployment entry point.
if locked_output="$(bash -c 'function omarchy-shell { printf '\''%s\n'\'' '\''{"locked":true,"requested":true,"secure":true}'\''; }; function timeout { shift; "$@"; }; source /dev/stdin; ensure_shell_unlocked' < <(sed -n '/^ensure_shell_unlocked() {/,/^}/p' "$plugin_dir/setup") 2>&1)"; then
  printf '%s\n' "Setup accepted an active Omarchy lockscreen" >&2
  exit 1
fi
grep -Fq 'Unlock the session first (Omarchy issue #9441).' <<<"$locked_output"

python3 -B - "$plugin_dir" <<'PY'
import pathlib, re, sys
root = pathlib.Path(sys.argv[1])
for path in [*root.glob('*.qml'), *root.glob('assets/*.svg')]:
    for match in re.finditer(r'#[0-9a-fA-F]{3,8}\b', path.read_text()):
        if path.suffix == '.svg' and match[0].lower() in ('#fff', '#ffffff'):
            continue
        raise SystemExit(f'Hard-coded display color: {path.name}: {match[0]}')
PY

printf '%s\n' "All Cetra plugin checks passed."
