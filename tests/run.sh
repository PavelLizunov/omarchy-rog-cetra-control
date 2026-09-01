#!/usr/bin/env bash

set -euo pipefail

plugin_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${XDG_RUNTIME_DIR:-/tmp}/slovn-cetra-test.$$"
trap 'rm -rf "$build_dir"' EXIT
mkdir -p "$build_dir"

omarchy plugin validate "$plugin_dir"

cc -O2 -Wall -Wextra -Werror \
  -o "$build_dir/cetra-status" \
  "$plugin_dir/cetra-status.c" \
  $(pkg-config --cflags --libs hidapi-hidraw)

test "$("$build_dir/cetra-status" --selftest)" = "ok"

if grep -REn '#[0-9a-fA-F]{6}|#[0-9a-fA-F]{8}' \
  "$plugin_dir"/*.qml "$plugin_dir/assets"/*.svg | grep -v '#fff'; then
  printf '%s\n' "Hard-coded display color found" >&2
  exit 1
fi

printf '%s\n' "All Cetra plugin checks passed."
