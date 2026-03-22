#!/bin/bash
# Test Cassilda mode in Emacs with LSP support

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
EXAMPLE_FILE="${SCRIPT_DIR}/../wasm/examples/103-taylor-series.cld"

# Launch Emacs with cassilda-mode loaded and auto-start LSP
emacs -Q \
  --eval "(add-to-list 'load-path \"${SCRIPT_DIR}\")" \
  --eval "(require 'cassilda-mode)" \
  --eval "(add-hook 'cassilda-mode-hook #'eglot-ensure)" \
  --eval "(setq eglot-events-buffer-size 0)" \
  "${EXAMPLE_FILE}"
