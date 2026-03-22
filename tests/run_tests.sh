#!/usr/bin/env bash
set -euo pipefail
CASSILDA="${1:?Usage: run_tests.sh <path-to-cassilda>}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
GOLDENS="$SCRIPT_DIR/goldens"
EXAMPLES="$SCRIPT_DIR/../wasm/examples"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

pass=0 fail=0 missing=0
for src in "$EXAMPLES"/*.cld; do
    name="$(basename "$src")"
    golden="$GOLDENS/$name"
    if [ ! -f "$golden" ]; then
        echo "MISSING  $name (run 'make update-goldens')"
        missing=$((missing + 1))
        continue
    fi
    cp "$src" "$TMPDIR/$name"
    "$CASSILDA" process "$TMPDIR/$name"
    if diff -u "$golden" "$TMPDIR/$name" > "$TMPDIR/$name.diff" 2>&1; then
        echo "PASS     $name"
        pass=$((pass + 1))
    else
        echo "FAIL     $name"
        cat "$TMPDIR/$name.diff"
        fail=$((fail + 1))
    fi
done
echo ""
echo "Results: $pass passed, $fail failed, $missing missing"
[ "$fail" -eq 0 ] && [ "$missing" -eq 0 ]
