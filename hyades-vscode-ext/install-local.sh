#!/bin/bash
#
# Install the Cassilda VSCode extension locally for testing
#
# Usage: ./install-local.sh [--insiders]
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Determine target directory
if [[ "$1" == "--insiders" ]]; then
    EXT_DIR="$HOME/.vscode-insiders/extensions"
    VSCODE_NAME="VSCode Insiders"
else
    EXT_DIR="$HOME/.vscode/extensions"
    VSCODE_NAME="VSCode"
fi

# Get extension info from package.json
PUBLISHER=$(grep '"publisher"' "$SCRIPT_DIR/package.json" | sed 's/.*: *"\([^"]*\)".*/\1/')
NAME=$(grep '"name"' "$SCRIPT_DIR/package.json" | head -1 | sed 's/.*: *"\([^"]*\)".*/\1/')
VERSION=$(grep '"version"' "$SCRIPT_DIR/package.json" | sed 's/.*: *"\([^"]*\)".*/\1/')

# Use extension ID without version to avoid obsolete marking
TARGET_DIR="$EXT_DIR/$PUBLISHER.$NAME"

echo "Installing $PUBLISHER.$NAME v$VERSION to $VSCODE_NAME..."
echo "  Source: $SCRIPT_DIR"
echo "  Target: $TARGET_DIR (symlink)"

# Create extensions directory if it doesn't exist
mkdir -p "$EXT_DIR"

# Remove any existing installations (both versioned and unversioned)
echo "  Removing any existing installations..."
rm -rf "$EXT_DIR/$PUBLISHER.$NAME"*

# Create symlink to source directory
echo "  Creating symlink..."
ln -sf "$SCRIPT_DIR" "$TARGET_DIR"
