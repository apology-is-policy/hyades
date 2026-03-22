#!/bin/bash
# build-wasm.sh - Build Hyades as WebAssembly
#
# Prerequisites:
#   - Emscripten SDK installed and activated (source emsdk_env.sh)
#
# Usage:
#   ./build-wasm.sh          # Build release
#   ./build-wasm.sh debug    # Build debug (larger, with symbols)
#   ./build-wasm.sh clean    # Clean build directory

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
OUTPUT_DIR="$SCRIPT_DIR/dist"

# Check for Emscripten
if ! command -v emcc &> /dev/null; then
    echo "Error: Emscripten not found!"
    echo "Please install and activate Emscripten SDK:"
    echo "  git clone https://github.com/emscripten-core/emsdk.git"
    echo "  cd emsdk && ./emsdk install latest && ./emsdk activate latest"
    echo "  source ./emsdk_env.sh"
    exit 1
fi

# Handle arguments
case "${1:-}" in
    clean)
        echo "Cleaning build directory..."
        rm -rf "$BUILD_DIR" "$OUTPUT_DIR"
        echo "Done."
        exit 0
        ;;
    debug)
        BUILD_TYPE="Debug"
        ;;
    *)
        BUILD_TYPE="Release"
        ;;
esac

echo "=== Building Hyades WASM ($BUILD_TYPE) ==="
echo "Emscripten: $(emcc --version | head -1)"

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo ""
echo "=== Configuring ==="
emcmake cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE

# Build
echo ""
echo "=== Building ==="
emmake make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4) VERBOSE=1

# Create output directory and copy files
echo ""
echo "=== Packaging ==="
mkdir -p "$OUTPUT_DIR"

# Copy WASM files
cp hyades.js "$OUTPUT_DIR/"
cp hyades.wasm "$OUTPUT_DIR/"

# Copy JS API wrapper
cp "$SCRIPT_DIR/hyades-api.js" "$OUTPUT_DIR/"

# Copy web app files
cp "$SCRIPT_DIR/index.html" "$OUTPUT_DIR/"
cp "$SCRIPT_DIR/cassilda-app.css" "$OUTPUT_DIR/"
cp "$SCRIPT_DIR/cassilda-app.js" "$OUTPUT_DIR/"
cp "$SCRIPT_DIR/cld.gif" "$OUTPUT_DIR/"
cp "$SCRIPT_DIR/examples-library.js" "$OUTPUT_DIR/"
cp "$SCRIPT_DIR/hyades-worker.js" "$OUTPUT_DIR/"

# Copy LLM documentation
cp "$SCRIPT_DIR/../llms.txt" "$OUTPUT_DIR/"
cp "$SCRIPT_DIR/../llms-full.txt" "$OUTPUT_DIR/"

# Also copy compat version if built
if [ -f "hyades-compat.js" ]; then
    cp hyades-compat.js "$OUTPUT_DIR/"
    cp hyades-compat.wasm "$OUTPUT_DIR/"
fi

echo ""
echo "=== Build Complete ==="
echo "Output files in: $OUTPUT_DIR"
echo ""
ls -la "$OUTPUT_DIR"
echo ""
echo "To test locally:"
echo "  cd $OUTPUT_DIR"
echo "  python3 -m http.server 8080"
echo "  # Then open http://localhost:8080"