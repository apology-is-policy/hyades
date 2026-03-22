#!/bin/bash
# Cassilda/Hyades Editor Integration Installer
# Installs LSP server, tree-sitter grammar, and cassilda binary
#
# Usage: ./install.sh [-i]
#   -i  Interactive mode (prompt before optional steps)

set -e

# Parse flags — auto-yes by default, -i for interactive
AUTO_YES=true
while getopts "iy" opt; do
    case $opt in
        i) AUTO_YES=false ;;
        y) AUTO_YES=true ;;  # kept for backward compat
        *) echo "Usage: $0 [-i]"; exit 1 ;;
    esac
done

echo "=== Cassilda Editor Integration Installer ==="
echo

# Detect OS
case "$(uname -s)" in
    Linux*)  OS=linux;;
    Darwin*) OS=macos;;
    *)       echo "Unsupported OS: $(uname -s)"; exit 1;;
esac

echo "Detected OS: $OS"

# Check for Node.js (required for LSP server)
if command -v node &> /dev/null; then
    NODE_VERSION=$(node --version)
    echo "Node.js: $NODE_VERSION"
else
    echo
    echo "WARNING: Node.js not found!"
    echo "  LSP features (hover, go-to-definition, diagnostics) require Node.js v18+"
    echo "  Syntax highlighting will still work without it."
    echo "  Install from: https://nodejs.org/"
    echo
fi

# Set paths
HYADES_DIR="$HOME/.local/share/hyades"
BIN_DIR="$HOME/.local/bin"

# Detect script directory (where the release package is)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "Installing from: $SCRIPT_DIR"
echo

# Check required files exist (bundle directory for dev, root for release)
LSP_DIR="$SCRIPT_DIR/lsp-server/bundle"
if [ ! -f "$LSP_DIR/server.js" ]; then
    LSP_DIR="$SCRIPT_DIR/lsp-server"
fi
if [ ! -f "$LSP_DIR/server.js" ]; then
    echo "Error: lsp-server/server.js not found in release package"
    exit 1
fi

if [ ! -f "$SCRIPT_DIR/bin/cassilda" ]; then
    echo "Error: bin/cassilda not found in release package"
    exit 1
fi

if [ ! -f "$SCRIPT_DIR/bin/hyades" ]; then
    echo "Error: bin/hyades not found in release package"
    exit 1
fi

# Create directories
echo "Creating directories..."
mkdir -p "$HYADES_DIR/lsp-server"
mkdir -p "$HYADES_DIR/tree-sitter-cassilda"
mkdir -p "$BIN_DIR"

# Install LSP server
echo "Installing LSP server..."
cp "$LSP_DIR/server.js" "$HYADES_DIR/lsp-server/"
cp "$LSP_DIR/hyades-compat.js" "$HYADES_DIR/lsp-server/"
cp "$LSP_DIR/hyades-compat.wasm" "$HYADES_DIR/lsp-server/"

# Install binaries
echo "Installing binaries..."
cp "$SCRIPT_DIR/bin/cassilda" "$BIN_DIR/"
cp "$SCRIPT_DIR/bin/hyades" "$BIN_DIR/"
chmod +x "$BIN_DIR/cassilda" "$BIN_DIR/hyades"

# Install tree-sitter grammar
echo "Installing tree-sitter grammar..."
cp -r "$SCRIPT_DIR/tree-sitter-cassilda/"* "$HYADES_DIR/tree-sitter-cassilda/"

# Detect and configure editors
echo
echo "=== Editor Configuration ==="

# Helix
if command -v hx &> /dev/null; then
    echo
    echo "Helix detected! Installing grammar and queries..."

    HELIX_CONFIG="$HOME/.config/helix"
    HELIX_RUNTIME="$HELIX_CONFIG/runtime"
    mkdir -p "$HELIX_RUNTIME/grammars"
    mkdir -p "$HELIX_RUNTIME/queries/cassilda"

    # Copy grammar (may be .so or .dylib depending on platform)
    if [ -f "$SCRIPT_DIR/tree-sitter-cassilda/cassilda.so" ]; then
        cp "$SCRIPT_DIR/tree-sitter-cassilda/cassilda.so" "$HELIX_RUNTIME/grammars/"
    elif [ -f "$SCRIPT_DIR/tree-sitter-cassilda/cassilda.dylib" ]; then
        cp "$SCRIPT_DIR/tree-sitter-cassilda/cassilda.dylib" "$HELIX_RUNTIME/grammars/cassilda.so"
    fi

    # Copy queries
    cp "$SCRIPT_DIR/tree-sitter-cassilda/queries/"*.scm "$HELIX_RUNTIME/queries/cassilda/"

    echo "  Grammar installed to: $HELIX_RUNTIME/grammars/"
    echo "  Queries installed to: $HELIX_RUNTIME/queries/cassilda/"

    # Configure languages.toml
    HELIX_LANGS="$HELIX_CONFIG/languages.toml"
    if [ -f "$HELIX_LANGS" ] && grep -q "cassilda" "$HELIX_LANGS"; then
        # Ensure .hy extension is included
        if ! grep -q '"hy"' "$HELIX_LANGS"; then
            if [ "$OS" = "macos" ]; then
                sed -i '' 's/file-types = \["cld"\]/file-types = ["cld", "hy"]/' "$HELIX_LANGS"
            else
                sed -i 's/file-types = \["cld"\]/file-types = ["cld", "hy"]/' "$HELIX_LANGS"
            fi
            echo "  Added .hy extension to languages.toml"
        else
            echo "  Cassilda config already in languages.toml"
        fi
    else
        echo "  Adding Cassilda config to languages.toml..."
        cat >> "$HELIX_LANGS" << HELIX_LANG_EOF

# ============================================================================
# Cassilda/Hyades Language Support (auto-added by install.sh)
# ============================================================================

[language-server.hyades-lsp]
command = "node"
args = ["$HYADES_DIR/lsp-server/server.js", "--stdio"]

[[language]]
name = "cassilda"
scope = "source.cassilda"
file-types = ["cld", "hy"]
comment-token = "%"
language-servers = ["hyades-lsp"]
grammar = "cassilda"
formatter = { command = "cassilda", args = ["process", "-"] }
auto-format = false
indent = { tab-width = 4, unit = "    " }
HELIX_LANG_EOF
        echo "  Configured: $HELIX_LANGS"
    fi

    # Configure config.toml (optional LSP settings)
    HELIX_CONF="$HELIX_CONFIG/config.toml"
    if [ -f "$HELIX_CONF" ] && grep -q "inline-diagnostics" "$HELIX_CONF"; then
        echo "  LSP diagnostics already configured in config.toml"
    else
        echo "  Adding LSP settings to config.toml..."
        cat >> "$HELIX_CONF" << 'HELIX_CONF_EOF'

# ============================================================================
# LSP Settings (auto-added by install.sh)
# ============================================================================

[editor.lsp]
display-messages = true
display-inlay-hints = true

[editor.inline-diagnostics]
cursor-line = "hint"
other-lines = "error"
HELIX_CONF_EOF
        echo "  Configured: $HELIX_CONF"
    fi

    # Add Cassilda render keybinding (Space+Tab)
    if [ -f "$HELIX_CONF" ] && grep -q "keys.normal.space" "$HELIX_CONF"; then
        echo "  Keybindings already configured in config.toml"
    else
        echo "  Adding Cassilda render keybinding (Space+Tab) to config.toml..."
        cat >> "$HELIX_CONF" << 'HELIX_KEYS_EOF'

# ============================================================================
# Cassilda Keybindings (auto-added by install.sh)
# ============================================================================

[keys.normal.space]
tab = ["select_all", ":pipe cassilda --find-config --filename %{buffer_name} process -", "collapse_selection"]
HELIX_KEYS_EOF
        echo "  Configured: Space+Tab to render with Cassilda"
    fi
fi

# Neovim
if command -v nvim &> /dev/null; then
    echo
    echo "Neovim detected!"
    echo
    echo "  Add to your init.lua (or copy editor-configs/nvim-cassilda.lua):"
    echo "  See: $SCRIPT_DIR/editor-configs/nvim-cassilda.lua"

    # Optionally install tree-sitter parser for neovim
    NVIM_PARSER="$HOME/.local/share/nvim/site/parser"
    NVIM_QUERIES="$HOME/.local/share/nvim/site/queries/cassilda"

    echo
    if [ "$AUTO_YES" = true ]; then
        REPLY="y"
    else
        read -p "  Install tree-sitter grammar for Neovim? [y/N] " -n 1 -r
        echo
    fi
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        mkdir -p "$NVIM_PARSER"
        mkdir -p "$NVIM_QUERIES"

        # Copy grammar (may be .so or .dylib depending on platform)
        if [ -f "$SCRIPT_DIR/tree-sitter-cassilda/cassilda.so" ]; then
            cp "$SCRIPT_DIR/tree-sitter-cassilda/cassilda.so" "$NVIM_PARSER/"
        elif [ -f "$SCRIPT_DIR/tree-sitter-cassilda/cassilda.dylib" ]; then
            cp "$SCRIPT_DIR/tree-sitter-cassilda/cassilda.dylib" "$NVIM_PARSER/cassilda.so"
        fi

        cp "$SCRIPT_DIR/tree-sitter-cassilda/queries/"*.scm "$NVIM_QUERIES/"
        echo "  Tree-sitter grammar installed for Neovim"

        # Configure Neovim init.lua
        NVIM_CONFIG="$HOME/.config/nvim"
        NVIM_INIT="$NVIM_CONFIG/init.lua"
        mkdir -p "$NVIM_CONFIG"

        # Check if cassilda config already exists
        if [ -f "$NVIM_INIT" ] && grep -q "cassilda" "$NVIM_INIT"; then
            # Ensure .hy extension is included
            if ! grep -q 'hy.*=.*"cassilda"' "$NVIM_INIT"; then
                if [ "$OS" = "macos" ]; then
                    sed -i '' 's/extension = { cld = "cassilda" }/extension = { cld = "cassilda", hy = "cassilda" }/' "$NVIM_INIT"
                else
                    sed -i 's/extension = { cld = "cassilda" }/extension = { cld = "cassilda", hy = "cassilda" }/' "$NVIM_INIT"
                fi
                echo "  Added .hy extension to init.lua"
            else
                echo "  Cassilda config already in init.lua"
            fi
        else
            echo "  Adding Cassilda config to init.lua..."
            cat >> "$NVIM_INIT" << 'NVIM_EOF'

-- ============================================================================
-- Cassilda/Hyades Language Support (auto-added by install.sh)
-- ============================================================================

-- Colorscheme (if not already set)
if vim.g.colors_name == nil then
    vim.cmd("colorscheme habamax")  -- Good built-in dark theme
end
vim.opt.termguicolors = true

-- Filetype detection
vim.filetype.add({
    extension = { cld = "cassilda", hy = "cassilda" },
})

-- Tree-sitter highlighting for cassilda
vim.api.nvim_create_autocmd("FileType", {
    pattern = "cassilda",
    callback = function()
        pcall(vim.treesitter.start)
    end,
})

-- LSP configuration
vim.api.nvim_create_autocmd("FileType", {
    pattern = "cassilda",
    callback = function()
        local lsp_path = vim.fn.expand("~/.local/share/hyades/lsp-server/server.js")
        if vim.fn.filereadable(lsp_path) == 1 then
            vim.lsp.start({
                name = "hyades-lsp",
                cmd = { "node", lsp_path, "--stdio" },
                root_dir = vim.fn.getcwd(),
            })
        end
    end,
})

-- LSP keybindings
vim.api.nvim_create_autocmd("LspAttach", {
    callback = function(args)
        local opts = { buffer = args.buf }
        vim.keymap.set("n", "gd", vim.lsp.buf.definition, opts)
        vim.keymap.set("n", "K", vim.lsp.buf.hover, opts)
        vim.keymap.set("n", "gr", vim.lsp.buf.references, opts)
        vim.keymap.set("n", "[d", vim.diagnostic.goto_prev, opts)
        vim.keymap.set("n", "]d", vim.diagnostic.goto_next, opts)
    end,
})
NVIM_EOF
            echo "  Neovim configured: $NVIM_INIT"
        fi
    fi
fi

# Install documentation
echo
echo "=== Documentation ==="
DOCS_SRC="$SCRIPT_DIR/docs"
DOCS_DST="$HYADES_DIR/docs"
if [ -d "$DOCS_SRC" ] && ls "$DOCS_SRC"/*.cld &>/dev/null; then
    mkdir -p "$DOCS_DST"
    cp "$DOCS_SRC"/*.cld "$DOCS_DST/"
    echo "Installed docs to $DOCS_DST"

    # Render docs using freshly installed cassilda
    echo "Rendering documentation..."
    for f in "$DOCS_DST"/*.cld; do
        [ -f "$f" ] || continue
        "$BIN_DIR/cassilda" process "$f" 2>/dev/null || true
    done
else
    echo "No .cld docs found in release package (skipping)"
fi

echo
echo "=== Installation Complete ==="
echo
echo "Installed:"
echo "  - Binaries:    $BIN_DIR/cassilda, $BIN_DIR/hyades"
echo "  - LSP server:  $HYADES_DIR/lsp-server/server.js"
echo "  - Tree-sitter: $HYADES_DIR/tree-sitter-cassilda/"

# Verification test
echo
echo "=== Verification ==="
if OUTPUT=$(printf '%s\n' '\intersect_rules{\begin[60]{vbox}\child[center]{\figlet{Hyades Test}}\child[1]{}\child[center]{\begin{hbox}\child{\term_color[fg=black, bg=white]{\Boxed{$ f(x) = \sum_{k=0}^{n} \frac{f^{(k)}(a)}{k!}(x-a)^{k} + R_n(x) $}}}\end{hbox}}\child[1]{}\child[center]{\boxed{$ R_n(x) = \frac{f^{(n+1)}(\xi)}{(n+1)!}(x-a)^{n+1} $}}\child[1]{}\child[center]{\begin{hbox}\child[intrinsic]{\term_color[fg=red]{\Boxed{RED}}}\child[intrinsic]{ }\child[intrinsic]{\term_color[fg=green]{\Boxed{GREEN}}}\child[intrinsic]{ }\child[intrinsic]{\term_color[fg=blue]{\Boxed{BLUE}}}\end{hbox}}\end{vbox}}' | "$BIN_DIR/hyades" 2>&1); then
    echo "hyades: OK"
    echo "$OUTPUT" | sed 's/^/  /'
else
    echo "hyades: FAILED"
fi

# Add to PATH if not already there
if [[ ":$PATH:" != *":$BIN_DIR:"* ]]; then
    # Detect shell config file
    SHELL_NAME=$(basename "$SHELL")
    case "$SHELL_NAME" in
        zsh)  SHELL_RC="$HOME/.zshrc" ;;
        bash) SHELL_RC="$HOME/.bashrc" ;;
        fish) SHELL_RC="$HOME/.config/fish/config.fish" ;;
        *)    SHELL_RC="" ;;
    esac

    if [ -n "$SHELL_RC" ]; then
        # Check if already configured
        if [ -f "$SHELL_RC" ] && grep -q "$BIN_DIR" "$SHELL_RC"; then
            echo
            echo "PATH already configured in $SHELL_RC"
        else
            echo
            echo "Adding $BIN_DIR to PATH in $SHELL_RC..."
            if [ "$SHELL_NAME" = "fish" ]; then
                echo "fish_add_path $BIN_DIR" >> "$SHELL_RC"
            else
                echo "export PATH=\"$BIN_DIR:\$PATH\"" >> "$SHELL_RC"
            fi
            echo "  Restart your shell or run: source $SHELL_RC"
        fi
    else
        echo
        echo "Add $BIN_DIR to your PATH manually:"
        echo "  export PATH=\"$BIN_DIR:\$PATH\""
    fi
else
    echo
    echo "$BIN_DIR is already in PATH"
fi

echo
echo "Restart your editor to activate Cassilda support!"
