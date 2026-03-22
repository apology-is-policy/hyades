#!/bin/bash
# Test Cassilda in Helix with tree-sitter highlighting and LSP

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
EXAMPLE_FILE="${1:-${SCRIPT_DIR}/../wasm/examples/103-taylor-series.cld}"

echo "Opening $EXAMPLE_FILE in Helix..."
echo "Features:"
echo "  - Tree-sitter syntax highlighting"
echo "  - LSP diagnostics, hover (K), go-to-def (gd)"
echo ""

hx "$EXAMPLE_FILE"
