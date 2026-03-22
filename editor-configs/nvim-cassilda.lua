-- Cassilda/Hyades LSP configuration for Neovim
-- Usage: nvim -u /path/to/nvim-cassilda.lua file.cld (or file.hy)

-- Set up filetype detection for .cld and .hy files
vim.filetype.add({
  extension = {
    cld = "cassilda",
    hy = "cassilda",
  },
})

-- Tree-sitter highlighting
vim.api.nvim_create_autocmd("FileType", {
  pattern = "cassilda",
  callback = function()
    -- Enable tree-sitter highlighting if parser is available
    local ok = pcall(vim.treesitter.start)
    if not ok then
      -- Fall back to TeX syntax (close enough for basic highlighting)
      vim.cmd("runtime! syntax/tex.vim")
    end
  end,
})

-- LSP server path (relative to this config file)
local script_path = debug.getinfo(1, "S").source:sub(2)
local lsp_dir = vim.fn.fnamemodify(script_path, ":h")
local server_path = lsp_dir .. "/server.js"

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

-- ============================================================================
-- Cassilda Render Command
-- ============================================================================

-- Find the cassilda binary (check common locations)
local function find_cassilda()
  local paths = {
    vim.fn.expand("~/.local/bin/cassilda"),
    vim.fn.expand("~/bin/cassilda"),
    "/usr/local/bin/cassilda",
    "/opt/homebrew/bin/cassilda",
    -- Relative to project (for development)
    vim.fn.getcwd() .. "/build/cassilda",
  }
  for _, path in ipairs(paths) do
    if vim.fn.executable(path) == 1 then
      return path
    end
  end
  -- Fall back to PATH
  if vim.fn.executable("cassilda") == 1 then
    return "cassilda"
  end
  return nil
end

-- Render the current file (process @cassilda: references)
local function cassilda_render()
  local cassilda = find_cassilda()
  if not cassilda then
    vim.notify("cassilda binary not found. Install it or add to PATH.", vim.log.levels.ERROR)
    return
  end

  local file = vim.fn.expand("%:p")
  if file == "" then
    vim.notify("No file to render", vim.log.levels.WARN)
    return
  end

  -- Save the file first
  vim.cmd("silent write")

  -- Get cursor position to restore later
  local cursor = vim.api.nvim_win_get_cursor(0)

  -- Run cassilda process
  local result = vim.fn.system({ cassilda, "process", file })
  local exit_code = vim.v.shell_error

  if exit_code ~= 0 then
    vim.notify("Cassilda render failed:\n" .. result, vim.log.levels.ERROR)
    return
  end

  -- Reload the buffer to show rendered output
  vim.cmd("edit!")

  -- Restore cursor position
  pcall(vim.api.nvim_win_set_cursor, 0, cursor)

  vim.notify("Rendered: " .. vim.fn.expand("%:t"), vim.log.levels.INFO)
end

-- Create user command
vim.api.nvim_create_user_command("CassildaRender", cassilda_render, {
  desc = "Render @cassilda: references in the current file",
})

-- Keybinding for render (leader+r)
vim.api.nvim_create_autocmd("FileType", {
  pattern = "cassilda",
  callback = function()
    vim.keymap.set("n", "<leader>r", cassilda_render, {
      buffer = true,
      desc = "Render Cassilda file",
    })
  end,
})

-- Optional: Render on save (uncomment to enable)
-- vim.api.nvim_create_autocmd("BufWritePost", {
--   pattern = {"*.cld", "*.hy"},
--   callback = cassilda_render,
-- })

print("Cassilda LSP config loaded. Keybindings:")
print("  gd       - Go to definition")
print("  K        - Hover info")
print("  gr       - Find references")
print("  [d/]d    - Previous/next diagnostic")
print("  <Space>e - Show diagnostic float")
print("  <Space>r - Render file (process @cassilda: refs)")
print("  :CassildaRender - Render command")
