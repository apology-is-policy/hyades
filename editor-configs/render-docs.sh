#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CASSILDA="${1:-$SCRIPT_DIR/bin/cassilda}"
DOCS="${2:-$SCRIPT_DIR/docs}"

if [ ! -x "$CASSILDA" ]; then
    echo "Error: cassilda not found at $CASSILDA"
    echo "Usage: render-docs.sh [path-to-cassilda]"
    exit 1
fi

count=0
for f in "$DOCS"/*.cld; do
    [ -f "$f" ] || continue
    name="$(basename "$f")"
    "$CASSILDA" process "$f"
    echo "  Rendered $name"
    count=$((count + 1))
done
echo "Rendered $count documents in $DOCS"
