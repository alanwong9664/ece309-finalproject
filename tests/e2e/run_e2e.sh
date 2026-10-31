#!/usr/bin/env bash
# tests/e2e/run_e2e.sh — PROVIDED.
#
# Runs ./miniharness on every scripts/*.script that has a matching
# tests/e2e/<name>.input, in a fresh copy of tests/e2e/workspace/ (never the
# top-level workspace/, which your own demo runs modify), and compares stdout
# byte-for-byte with tests/e2e/expected/<name>.out (after replacing the text
# of every "-> error]" line with <reason>, except load_skill errors).
# Optional <name>.args adds flags.
#
#   bash tests/e2e/run_e2e.sh build/miniharness            # check
#   bash tests/e2e/run_e2e.sh build/miniharness --update   # regenerate goldens
set -u
[ -x "${1:-}" ] || { echo "usage: run_e2e.sh path/to/miniharness [--update]  (binary not found: ${1:-})"; exit 2; }
BIN="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
UPDATE="${2:-}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
TMP="$(mktemp -d)"
cd "$ROOT"   # so ./skills (the default --skills) is the repo's skill library
trap 'rm -rf "$TMP"' EXIT

fail=0; total=0
for input in "$HERE"/*.input; do
  name="$(basename "$input" .input)"
  script="$ROOT/scripts/$name.script"
  total=$((total + 1))
  rm -rf "$TMP/ws" && cp -R "$HERE/workspace" "$TMP/ws"
  args=(--workspace "$TMP/ws")
  [ -f "$script" ] && args+=(--script "$script")   # no script: flags only (e.g. --tools)
  [ -f "$HERE/$name.args" ] && read -r -a extra < "$HERE/$name.args" && args+=("${extra[@]}")
  # Error wording is yours to choose, so error reasons are normalized away,
  # except load_skill errors: the DepGraph messages (cycle: / missing: /
  # unknown:) are specified exactly, so they are compared too.
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
