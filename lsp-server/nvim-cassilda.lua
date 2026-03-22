-- Cassilda/Hyades LSP configuration for Neovim
-- Usage: nvim -u /path/to/nvim-cassilda.lua file.cld

-- Set up filetype detection for .cld files
vim.filetype.add({
  extension = {
    cld = "cassilda",
  },
})

-- Basic syntax highlighting (before LSP kicks in)
vim.api.nvim_create_autocmd("FileType", {
  pattern = "cassilda",
  callback = function()
    -- Use TeX syntax as base (close enough for basic highlighting)
    vim.cmd("runtime! syntax/tex.vim")
  end,
})

-- LSP server path (relative to this config file)
local script_path = debug.getinfo(1, "S").source:sub(2)
local lsp_dir = vim.fn.fnamemodify(script_path, ":h")
local server_path = lsp_dir .. "/dist/server.js"

-- Configure the LSP
vim.api.nvim_create_autocmd("FileType", {
  pattern = "cassilda",
  callback = function()
    vim.lsp.start({
      name = "hyades-lsp",
      cmd = { "node", server_path, "--stdio" },
      root_dir = vim.fn.getcwd(),
      settings = {},
    })
  end,
})

-- Keybindings for LSP (when LSP attaches)
vim.api.nvim_create_autocmd("LspAttach", {
  callback = function(args)
    local opts = { buffer = args.buf }

    -- Go to definition
    vim.keymap.set("n", "gd", vim.lsp.buf.definition, opts)

    -- Hover
    vim.keymap.set("n", "K", vim.lsp.buf.hover, opts)

    -- Find references
    vim.keymap.set("n", "gr", vim.lsp.buf.references, opts)

    -- Completion (Ctrl+Space)
    vim.keymap.set("i", "<C-Space>", vim.lsp.buf.completion, opts)

    -- Show diagnostics
    vim.keymap.set("n", "<leader>e", vim.diagnostic.open_float, opts)
    vim.keymap.set("n", "[d", vim.diagnostic.goto_prev, opts)
    vim.keymap.set("n", "]d", vim.diagnostic.goto_next, opts)

    -- Rename symbol
    vim.keymap.set("n", "<leader>rn", vim.lsp.buf.rename, opts)
  end,
})

-- Show diagnostics inline
vim.diagnostic.config({
  virtual_text = true,
  signs = true,
  underline = true,
  update_in_insert = false,
})

-- Enable semantic tokens highlighting
vim.api.nvim_create_autocmd("LspAttach", {
  callback = function(args)
    local client = vim.lsp.get_client_by_id(args.data.client_id)
    if client and client.server_capabilities.semanticTokensProvider then
      -- Enable semantic highlighting
      vim.lsp.semantic_tokens.start(args.buf, args.data.client_id)
    end
  end,
})

-- Some nice defaults
vim.opt.number = true
vim.opt.relativenumber = true
vim.opt.signcolumn = "yes"
vim.opt.termguicolors = true
vim.opt.updatetime = 250

-- Leader key
vim.g.mapleader = " "

print("Cassilda LSP config loaded. Keybindings:")
print("  gd     - Go to definition")
print("  K      - Hover info")
print("  gr     - Find references")
print("  [d/]d  - Previous/next diagnostic")
print("  <Space>e - Show diagnostic float")
