#!/usr/bin/env bash
set -euo pipefail
CASSILDA="${1:?Usage: update_goldens.sh <path-to-cassilda>}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
GOLDENS="$SCRIPT_DIR/goldens"
EXAMPLES="$SCRIPT_DIR/../wasm/examples"

mkdir -p "$GOLDENS"
count=0
for src in "$EXAMPLES"/*.cld; do
    name="$(basename "$src")"
    cp "$src" "$GOLDENS/$name"
    "$CASSILDA" process "$GOLDENS/$name"
    count=$((count + 1))
done
echo "Updated $count goldens in $GOLDENS"
