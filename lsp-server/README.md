# Hyades Language Server

A Language Server Protocol (LSP) implementation for Hyades documents (`.cld` files).

## Features

- **Diagnostics**: Real-time syntax error reporting with line:column locations
- **Completion**: Autocomplete for commands, macros, and user-defined symbols
- **Go to Definition**: Navigate to macro and lambda definitions
- **Hover**: View documentation and signatures on hover
- **Find References**: Find all usages of a symbol
- **Document Outline**: View document structure via symbols

## Installation

```bash
cd lsp-server
npm install
npm run build
```

## Usage

The server can run in stdio mode for integration with various editors:

```bash
node dist/server.js --stdio
```

## Editor Integration

### VSCode

Install the Hyades extension from the VSCode marketplace, or use the extension in `../vscode-extension/`.

The extension automatically starts the language server.

### Neovim (nvim-lspconfig)

Add to your Neovim config:

```lua
local lspconfig = require('lspconfig')
local configs = require('lspconfig.configs')

-- Define the Hyades language server
if not configs.hyades then
  configs.hyades = {
    default_config = {
      cmd = { 'node', '/path/to/hyades/lsp-server/dist/server.js', '--stdio' },
      filetypes = { 'hyades', 'cld' },
      root_dir = lspconfig.util.find_git_ancestor,
      settings = {},
    },
  }
end

-- Enable the server
lspconfig.hyades.setup {}
```

Also add file type detection:

```lua
vim.filetype.add({
  extension = {
    cld = 'hyades',
  },
})
```

### Emacs (lsp-mode)

Add to your Emacs config:

```elisp
;; Define hyades-mode if not already defined
(define-derived-mode hyades-mode fundamental-mode "Hyades"
  "Major mode for editing Hyades documents.")

(add-to-list 'auto-mode-alist '("\\.cld\\'" . hyades-mode))

;; Register the language server
(require 'lsp-mode)

(lsp-register-client
  (make-lsp-client
    :new-connection (lsp-stdio-connection
                      '("node" "/path/to/hyades/lsp-server/dist/server.js" "--stdio"))
    :major-modes '(hyades-mode)
    :server-id 'hyades-ls))

;; Enable LSP for hyades-mode
(add-hook 'hyades-mode-hook #'lsp)
```

### Sublime Text (LSP package)

1. Install the LSP package via Package Control
2. Add to LSP settings (Preferences > Package Settings > LSP > Settings):

```json
{
  "clients": {
    "hyades": {
      "enabled": true,
      "command": ["node", "/path/to/hyades/lsp-server/dist/server.js", "--stdio"],
      "selector": "source.hyades",
      "languageId": "hyades"
    }
  }
}
```

3. Create a `.sublime-syntax` file for `.cld` files

### Helix

Add to `~/.config/helix/languages.toml`:

```toml
[[language]]
name = "hyades"
scope = "source.hyades"
file-types = ["cld"]
roots = []

[language-server.hyades]
command = "node"
args = ["/path/to/hyades/lsp-server/dist/server.js", "--stdio"]

[[language]]
name = "hyades"
language-servers = ["hyades"]
```

### Kate / KTextEditor

1. Go to Settings > Configure Kate > LSP Client
2. Add server configuration:

```json
{
  "servers": {
    "hyades": {
      "command": ["node", "/path/to/hyades/lsp-server/dist/server.js", "--stdio"],
      "highlightingModeRegex": "^Hyades$"
    }
  }
}
```

## Development

### Building

```bash
npm run build
```

### Watching for changes

```bash
npm run watch
```

### Testing

```bash
npm test
```

## Architecture

The language server uses a WASM-compiled version of the Hyades parser:

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│  Editor/IDE     │────▶│  Language Server │────▶│  Hyades WASM    │
│  (LSP Client)   │◀────│  (TypeScript)    │◀────│  (Parser)       │
└─────────────────┘     └──────────────────┘     └─────────────────┘
        │                       │                       │
        │ stdio/socket          │ JSON-RPC             │ JSON
        │                       │                       │
```

### Files

- `src/server.ts` - Main LSP server implementation
- `src/hyades-wasm.ts` - TypeScript bindings for WASM module
- `hyades.wasm` - Compiled parser (copied from `wasm/build/`)

## Protocol Support

| Feature | Status |
|---------|--------|
| textDocument/publishDiagnostics | ✅ |
| textDocument/completion | ✅ |
| textDocument/definition | ✅ |
| textDocument/hover | ✅ |
| textDocument/references | ✅ |
| textDocument/documentSymbol | ✅ |
| textDocument/formatting | ❌ Planned |
| textDocument/rename | ❌ Planned |

## License

MIT
