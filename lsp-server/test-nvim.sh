#!/bin/bash
# Test Cassilda LSP in Neovim

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
EXAMPLE_FILE="${1:-${SCRIPT_DIR}/../wasm/examples/103-taylor-series.cld}"

# Launch Neovim with minimal config + our LSP setup
nvim -u "${SCRIPT_DIR}/nvim-cassilda.lua" "${EXAMPLE_FILE}"
