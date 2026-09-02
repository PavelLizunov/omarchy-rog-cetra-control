#!/usr/bin/env bash

set -euo pipefail

plugin_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${XDG_RUNTIME_DIR:-/tmp}/slovn-cetra-test.$$"
trap 'rm -rf "$build_dir"' EXIT
mkdir -p "$build_dir"

python3 -m json.tool "$plugin_dir/manifest.json" >/dev/null
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
test "$("$build_dir/cetra-watch" --selftest)" = "ok"
grep -Fq 'Omarchy issue #9441' "$plugin_dir/setup"
grep -Fq 'signal(SIGPIPE, SIG_IGN)' "$plugin_dir/cetra-watch.c"
if grep -Eq '0x33|05 33|mic (live|muted)' "$plugin_dir/cetra-watch.c" "$plugin_dir/Cetra.qml"; then
  printf '%s\n' "Unsupported software microphone state command found" >&2
  exit 1
fi
if grep -Eq 'mic_muted|0xcc, 0x70|proven hardware toggle' "$plugin_dir/cetra-watch.c" "$plugin_dir/Cetra.qml" "$plugin_dir/README.md"; then
  printf '%s\n' "Unreliable microphone state inference found" >&2
  exit 1
fi

call_filter='any(.[]; . as $s | (.properties // {}) as $p | (["application.name", "application.process.binary", "application.id", "application.icon_name", "pipewire.access.portal.app_id", "node.name", "media.name", "media.filename"] | map(($p[.] // "") | tostring) | join(" ")) as $id | ($s.corked != true) and (($id | test("easy[ _-]?effects|pw-(record|cat)|voxtype|recognition|keepalive|/dev/null"; "i") | not) and (($id | test("(^|[^[:alnum:]_])(webrtc|chrom(e|ium)( input)?|firefox|discord|vesktop|steam(webhelper)?|telegram|zoom|brave|vivaldi|microsoft-edge)([^[:alnum:]_]|$)"; "i")) or (($p["media.role"] // "") | test("^(phone|communication)$"; "i")))))'
chromium_fixture='[{"corked":false,"properties":{"application.name":"Chromium input","application.process.binary":"chromium","media.name":"RecordStream"}}]'
keepalive_fixture='[{"corked":false,"properties":{"application.name":"pw-record","node.name":"pw-record","media.filename":"/dev/null","target.object":"easyeffects_source"}}]'
test "$(jq -nr --argjson streams "$chromium_fixture" '$streams | '"$call_filter")" = "true"
test "$(jq -nr --argjson streams "$keepalive_fixture" '$streams | '"$call_filter")" = "false"

if locked_output="$(bash -c 'function omarchy-shell { printf '\''%s\n'\'' '\''{"locked":true,"requested":true,"secure":true}'\''; }; export -f omarchy-shell; exec "$1"' _ "$plugin_dir/setup" 2>&1)"; then
  printf '%s\n' "Setup accepted an active Omarchy lockscreen" >&2
  exit 1
fi
grep -Fq 'Unlock the session first (Omarchy issue #9441).' <<<"$locked_output"

if grep -REn '#[0-9a-fA-F]{6}|#[0-9a-fA-F]{8}' \
  "$plugin_dir"/*.qml "$plugin_dir/assets"/*.svg | grep -v '#fff'; then
  printf '%s\n' "Hard-coded display color found" >&2
  exit 1
fi

printf '%s\n' "All Cetra plugin checks passed."
