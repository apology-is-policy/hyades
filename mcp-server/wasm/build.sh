#!/bin/bash
set -e
cd "$(dirname "$0")"
mkdir -p build && cd build
emcmake cmake .. -DCMAKE_BUILD_TYPE=Release
emmake make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
echo ""
echo "Build complete:"
ls -lh hyades-render.js hyades-render.wasm
