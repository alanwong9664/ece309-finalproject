#!/usr/bin/env bash

set -u
[ -x "${1:-}" ] || { echo "usage: run_e2e.sh path/to/miniharness [--update]  (binary not found: ${1:-})"; exit 2; }
BIN="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
UPDATE="${2:-}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
TMP="$(mktemp -d)"
cd "$ROOT"
trap 'rm -rf "$TMP"' EXIT

fail=0; total=0
for input in "$HERE"/*.input; do
  name="$(basename "$input" .input)"
  script="$ROOT/scripts/$name.script"
  total=$((total + 1))
  rm -rf "$TMP/ws" && cp -R "$HERE/workspace" "$TMP/ws"
  args=(--workspace "$TMP/ws")
  [ -f "$script" ] && args+=(--script "$script")
  [ -f "$HERE/$name.args" ] && read -r -a extra < "$HERE/$name.args" && args+=("${extra[@]}")

  "$BIN" "${args[@]}" < "$input" 2>&1 | sed -E '/\[tool load_skill /!s/(-> error\]) .*/\1 <reason>/' > "$TMP/$name.out"
  echo "[exit ${PIPESTATUS[0]}]" >> "$TMP/$name.out"
  if [ "$UPDATE" = "--update" ]; then
    cp "$TMP/$name.out" "$HERE/expected/$name.out"; echo "[UPDATED] $name"
  elif diff -u "$HERE/expected/$name.out" "$TMP/$name.out" > "$TMP/diff"; then
    echo "[PASS] e2e_$name"
  else
    echo "[FAIL] e2e_$name"; cat "$TMP/diff"; fail=$((fail + 1))
  fi
done
echo "e2e: $((total - fail)) / $total passed"
[ "$fail" -eq 0 ]
