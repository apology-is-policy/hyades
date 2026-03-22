# Cassilda/Hyades Editor Integration Installer for Windows
# Run: powershell -ExecutionPolicy Bypass -File install.ps1 [-y]
#   -y  Auto-yes to all prompts (for scripted installs)

param(
    [switch]$y = $false
)

$ErrorActionPreference = "Stop"

Write-Host "=== Cassilda Editor Integration Installer ===" -ForegroundColor Cyan
Write-Host

# Set paths
$HyadesDir = "$env:USERPROFILE\AppData\Local\hyades"
$BinDir = "$env:USERPROFILE\bin"

# Detect script directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host "Installing from: $ScriptDir"

# Check for Node.js (required for LSP server)
$NodePath = Get-Command node -ErrorAction SilentlyContinue
if ($NodePath) {
    $NodeVersion = & node --version
    Write-Host "Node.js: $NodeVersion"
} else {
    Write-Host
    Write-Host "WARNING: Node.js not found!" -ForegroundColor Yellow
    Write-Host "  LSP features (hover, go-to-definition, diagnostics) require Node.js v18+"
    Write-Host "  Syntax highlighting will still work without it."
    Write-Host "  Install from: https://nodejs.org/"
    Write-Host
}

# Check required files (bundle directory for dev, root for release)
$LspDir = "$ScriptDir\lsp-server\bundle"
if (-not (Test-Path "$LspDir\server.js")) {
    $LspDir = "$ScriptDir\lsp-server"
}
if (-not (Test-Path "$LspDir\server.js")) {
    Write-Host "Error: lsp-server\server.js not found" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path "$ScriptDir\bin\cassilda.exe")) {
    Write-Host "Error: bin\cassilda.exe not found" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path "$ScriptDir\bin\hyades.exe")) {
    Write-Host "Error: bin\hyades.exe not found" -ForegroundColor Red
    exit 1
}

# Create directories
Write-Host "Creating directories..."
New-Item -ItemType Directory -Force -Path "$HyadesDir\lsp-server" | Out-Null
New-Item -ItemType Directory -Force -Path "$HyadesDir\tree-sitter-cassilda" | Out-Null
New-Item -ItemType Directory -Force -Path $BinDir | Out-Null

# Install LSP server
Write-Host "Installing LSP server..."
Copy-Item "$LspDir\server.js" "$HyadesDir\lsp-server\"
Copy-Item "$LspDir\hyades-compat.js" "$HyadesDir\lsp-server\"
Copy-Item "$LspDir\hyades-compat.wasm" "$HyadesDir\lsp-server\"

# Install binaries
Write-Host "Installing binaries..."
Copy-Item "$ScriptDir\bin\cassilda.exe" "$BinDir\"
Copy-Item "$ScriptDir\bin\hyades.exe" "$BinDir\"

# Install tree-sitter grammar
Write-Host "Installing tree-sitter grammar..."
Copy-Item -Recurse -Force "$ScriptDir\tree-sitter-cassilda\*" "$HyadesDir\tree-sitter-cassilda\"

Write-Host
Write-Host "=== Editor Configuration ===" -ForegroundColor Cyan

# Helix
$HelixPath = Get-Command hx -ErrorAction SilentlyContinue
if ($HelixPath) {
    Write-Host
    Write-Host "Helix detected! Installing grammar and queries..." -ForegroundColor Green

    $HelixConfig = "$env:APPDATA\helix"
    $HelixRuntime = "$HelixConfig\runtime"
    New-Item -ItemType Directory -Force -Path "$HelixRuntime\grammars" | Out-Null
    New-Item -ItemType Directory -Force -Path "$HelixRuntime\queries\cassilda" | Out-Null

    if (Test-Path "$ScriptDir\tree-sitter-cassilda\cassilda.dll") {
        Copy-Item "$ScriptDir\tree-sitter-cassilda\cassilda.dll" "$HelixRuntime\grammars\cassilda.dll"
    }

    Copy-Item "$ScriptDir\tree-sitter-cassilda\queries\*.scm" "$HelixRuntime\queries\cassilda\"

    Write-Host "  Grammar installed to: $HelixRuntime\grammars\"
    Write-Host "  Queries installed to: $HelixRuntime\queries\cassilda\"

    # Configure languages.toml
    $HelixLangs = "$HelixConfig\languages.toml"
    if ((Test-Path $HelixLangs) -and (Select-String -Path $HelixLangs -Pattern "cassilda" -Quiet)) {
        # Ensure .hy extension is included
        if (-not (Select-String -Path $HelixLangs -Pattern '"hy"' -Quiet)) {
            (Get-Content $HelixLangs) -replace 'file-types = \["cld"\]', 'file-types = ["cld", "hy"]' | Set-Content $HelixLangs
            Write-Host "  Added .hy extension to languages.toml"
        } else {
            Write-Host "  Cassilda config already in languages.toml"
        }
    } else {
        Write-Host "  Adding Cassilda config to languages.toml..."
        $LspPath = "$HyadesDir\lsp-server\server.js" -replace '\\', '/'
        $langConfig = @"

# ============================================================================
# Cassilda/Hyades Language Support (auto-added by install.ps1)
# ============================================================================

[language-server.hyades-lsp]
command = "node"
args = ["$LspPath", "--stdio"]

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
"@
        Add-Content -Path $HelixLangs -Value $langConfig
        Write-Host "  Configured: $HelixLangs"
    }

    # Configure config.toml
    $HelixConf = "$HelixConfig\config.toml"
    if ((Test-Path $HelixConf) -and (Select-String -Path $HelixConf -Pattern "inline-diagnostics" -Quiet)) {
        Write-Host "  LSP diagnostics already configured in config.toml"
    } else {
        Write-Host "  Adding LSP settings to config.toml..."
        $confConfig = @"

# ============================================================================
# LSP Settings (auto-added by install.ps1)
# ============================================================================

[editor.lsp]
display-messages = true
display-inlay-hints = true

[editor.inline-diagnostics]
cursor-line = "hint"
other-lines = "error"
"@
        Add-Content -Path $HelixConf -Value $confConfig
        Write-Host "  Configured: $HelixConf"
    }

    # Add Cassilda render keybinding (Space+Tab)
    if ((Test-Path $HelixConf) -and (Select-String -Path $HelixConf -Pattern "keys.normal.space" -Quiet)) {
        Write-Host "  Keybindings already configured in config.toml"
    } else {
        Write-Host "  Adding Cassilda render keybinding (Space+Tab) to config.toml..."
        $keysConfig = @"

# ============================================================================
# Cassilda Keybindings (auto-added by install.ps1)
# ============================================================================

[keys.normal.space]
tab = ["select_all", ":pipe cassilda --find-config --filename %{buffer_name} process -", "collapse_selection"]
"@
        Add-Content -Path $HelixConf -Value $keysConfig
        Write-Host "  Configured: Space+Tab to render with Cassilda"
    }
}

# Neovim
$NvimPath = Get-Command nvim -ErrorAction SilentlyContinue
if ($NvimPath) {
    Write-Host
    Write-Host "Neovim detected!" -ForegroundColor Green

    $NvimParser = "$env:LOCALAPPDATA\nvim-data\site\parser"
    $NvimQueries = "$env:LOCALAPPDATA\nvim-data\site\queries\cassilda"

    Write-Host
    if ($y) {
        $reply = "y"
    } else {
        $reply = Read-Host "  Install tree-sitter grammar for Neovim? [y/N]"
    }
    if ($reply -match "^[Yy]$") {
        New-Item -ItemType Directory -Force -Path $NvimParser | Out-Null
        New-Item -ItemType Directory -Force -Path $NvimQueries | Out-Null

        if (Test-Path "$ScriptDir\tree-sitter-cassilda\cassilda.dll") {
            Copy-Item "$ScriptDir\tree-sitter-cassilda\cassilda.dll" "$NvimParser\cassilda.dll"
        }

        Copy-Item "$ScriptDir\tree-sitter-cassilda\queries\*.scm" "$NvimQueries\"
        Write-Host "  Tree-sitter grammar installed for Neovim" -ForegroundColor Green

        # Configure Neovim init.lua
        $NvimConfig = "$env:LOCALAPPDATA\nvim"
        $NvimInit = "$NvimConfig\init.lua"
        New-Item -ItemType Directory -Force -Path $NvimConfig | Out-Null

        if ((Test-Path $NvimInit) -and (Select-String -Path $NvimInit -Pattern "cassilda" -Quiet)) {
            # Ensure .hy extension is included
            if (-not (Select-String -Path $NvimInit -Pattern 'hy.*=.*"cassilda"' -Quiet)) {
                (Get-Content $NvimInit) -replace 'extension = \{ cld = "cassilda" \}', 'extension = { cld = "cassilda", hy = "cassilda" }' | Set-Content $NvimInit
                Write-Host "  Added .hy extension to init.lua"
            } else {
                Write-Host "  Cassilda config already in init.lua"
            }
        } else {
            Write-Host "  Adding Cassilda config to init.lua..."
            $nvimLua = @"

-- ============================================================================
-- Cassilda/Hyades Language Support (auto-added by install.ps1)
-- ============================================================================

-- Colorscheme (if not already set)
if vim.g.colors_name == nil then
    vim.cmd("colorscheme habamax")
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
        local lsp_path = vim.fn.expand("~/AppData/Local/hyades/lsp-server/server.js")
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
"@
            Add-Content -Path $NvimInit -Value $nvimLua
            Write-Host "  Configured: $NvimInit"
        }
    }
}

# Install documentation
Write-Host
Write-Host "=== Documentation ===" -ForegroundColor Cyan
$DocsSrc = "$ScriptDir\docs"
$DocsDst = "$HyadesDir\docs"
if ((Test-Path $DocsSrc) -and (Get-ChildItem -Path $DocsSrc -Filter "*.cld" -ErrorAction SilentlyContinue)) {
    New-Item -ItemType Directory -Force -Path $DocsDst | Out-Null
    Copy-Item "$DocsSrc\*.cld" $DocsDst
    Write-Host "Installed docs to $DocsDst"

    # Render docs using freshly installed cassilda
    Write-Host "Rendering documentation..."
    Get-ChildItem -Path $DocsDst -Filter "*.cld" | ForEach-Object {
        & "$BinDir\cassilda.exe" process $_.FullName
        Write-Host "  Rendered $($_.Name)"
    }
} else {
    Write-Host "No .cld docs found in release package (skipping)"
}

Write-Host
Write-Host "=== Installation Complete ===" -ForegroundColor Green
Write-Host
Write-Host "Installed:"
Write-Host "  - Binaries:    $BinDir\cassilda.exe, $BinDir\hyades.exe"
Write-Host "  - LSP server:  $HyadesDir\lsp-server\server.js"
Write-Host "  - Tree-sitter: $HyadesDir\tree-sitter-cassilda\"

# Verification test
Write-Host
Write-Host "=== Verification ===" -ForegroundColor Cyan
try {
    $tempFile = [System.IO.Path]::GetTempFileName()
    [System.IO.File]::WriteAllText($tempFile, '\intersect_rules{\begin[60]{vbox}\child[center]{\figlet{Hyades Test}}\child[1]{}\child[center]{\begin{hbox}\child{\term_color[fg=black, bg=white]{\Boxed{$ f(x) = \sum_{k=0}^{n} \frac{f^{(k)}(a)}{k!}(x-a)^{k} + R_n(x) $}}}\end{hbox}}\child[1]{}\child[center]{\boxed{$ R_n(x) = \frac{f^{(n+1)}(\xi)}{(n+1)!}(x-a)^{n+1} $}}\child[1]{}\child[center]{\begin{hbox}\child[intrinsic]{\term_color[fg=red]{\Boxed{RED}}}\child[intrinsic]{ }\child[intrinsic]{\term_color[fg=green]{\Boxed{GREEN}}}\child[intrinsic]{ }\child[intrinsic]{\term_color[fg=blue]{\Boxed{BLUE}}}\end{hbox}}\end{vbox}}', (New-Object System.Text.UTF8Encoding $false))
    cmd /c "`"$BinDir\hyades.exe`" `"$tempFile`""
    if ($LASTEXITCODE -eq 0) {
        Write-Host "hyades: OK" -ForegroundColor Green
    } else {
        Write-Host "hyades: FAILED (exit code $LASTEXITCODE)" -ForegroundColor Red
    }
} catch {
    Write-Host "hyades: FAILED" -ForegroundColor Red
} finally {
    if ($tempFile) { Remove-Item $tempFile -ErrorAction SilentlyContinue }
}

# Add to PATH if not already there
$currentPath = [Environment]::GetEnvironmentVariable("PATH", "User")
if ($currentPath -notlike "*$BinDir*") {
    Write-Host
    Write-Host "Adding $BinDir to user PATH..." -ForegroundColor Yellow
    $newPath = "$BinDir;$currentPath"
    [Environment]::SetEnvironmentVariable("PATH", $newPath, "User")
    $env:PATH = "$BinDir;$env:PATH"
    Write-Host "  PATH updated. Restart your terminal for changes to take effect."
} else {
    Write-Host
    Write-Host "$BinDir is already in PATH"
}

# JuliaMono font installation
Write-Host
Write-Host "=== Font ===" -ForegroundColor Cyan
$needsRestart = $false
$FontDir = "$env:LOCALAPPDATA\Microsoft\Windows\Fonts"
$FontRegPath = "HKCU:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts"
$juliaMonoInstalled = (Test-Path "$FontDir\JuliaMono-Regular.ttf") -or (Test-Path "$env:WINDIR\Fonts\JuliaMono-Regular.ttf")

if ($juliaMonoInstalled) {
    Write-Host "JuliaMono already installed"
} else {
    Write-Host "Hyades works best with JuliaMono, a monospaced font with full Unicode coverage."
    if ($y) {
        $reply = "y"
    } else {
        $reply = Read-Host "  Install JuliaMono font? [y/N]"
    }
    if ($reply -match "^[Yy]$") {
        $tempZip = $null
        $tempDir = $null
        try {
            Write-Host "  Downloading JuliaMono..."
            $tempZip = [System.IO.Path]::GetTempFileName() + ".zip"
            $tempDir = Join-Path ([System.IO.Path]::GetTempPath()) "juliamono_install"
            [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
            Invoke-WebRequest -Uri "https://github.com/cormullion/juliamono/releases/latest/download/JuliaMono-ttf.zip" -OutFile $tempZip -UseBasicParsing

            Write-Host "  Extracting..."
            if (Test-Path $tempDir) { Remove-Item $tempDir -Recurse -Force }
            Expand-Archive -Path $tempZip -DestinationPath $tempDir

            # Install core weights per-user
            New-Item -ItemType Directory -Force -Path $FontDir | Out-Null
            $coreWeights = @(
                @{ File = "JuliaMono-Regular.ttf";       Name = "JuliaMono Regular (TrueType)" },
                @{ File = "JuliaMono-Bold.ttf";          Name = "JuliaMono Bold (TrueType)" },
                @{ File = "JuliaMono-RegularItalic.ttf";  Name = "JuliaMono Regular Italic (TrueType)" },
                @{ File = "JuliaMono-BoldItalic.ttf";     Name = "JuliaMono Bold Italic (TrueType)" }
            )
            $installed = 0
            foreach ($font in $coreWeights) {
                $src = Get-ChildItem -Path $tempDir -Filter $font.File -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
                if ($src) {
                    Copy-Item $src.FullName "$FontDir\$($font.File)" -Force
                    New-ItemProperty -Path $FontRegPath -Name $font.Name -Value "$FontDir\$($font.File)" -PropertyType String -Force | Out-Null
                    $installed++
                }
            }
            Write-Host "  Installed $installed font files to $FontDir" -ForegroundColor Green
            $juliaMonoInstalled = $true
            $needsRestart = $true
        } catch {
            Write-Host "  Failed to install JuliaMono: $_" -ForegroundColor Red
        } finally {
            if ($tempZip -and (Test-Path $tempZip)) { Remove-Item $tempZip -Force -ErrorAction SilentlyContinue }
            if ($tempDir -and (Test-Path $tempDir)) { Remove-Item $tempDir -Recurse -Force -ErrorAction SilentlyContinue }
        }
    }
}

# Configure Windows Terminal to use JuliaMono
if ($juliaMonoInstalled) {
    $wtSettings = $null
    $wtPaths = @(
        "$env:LOCALAPPDATA\Packages\Microsoft.WindowsTerminal_8wekyb3d8bbwe\LocalState\settings.json",
        "$env:LOCALAPPDATA\Microsoft\Windows Terminal\settings.json",
        "$env:LOCALAPPDATA\Packages\Microsoft.WindowsTerminalPreview_8wekyb3d8bbwe\LocalState\settings.json"
    )
    foreach ($p in $wtPaths) {
        if (Test-Path $p) { $wtSettings = $p; break }
    }

    if ($wtSettings) {
        # Check if already configured
        $wtJson = Get-Content $wtSettings -Raw | ConvertFrom-Json
        $alreadySet = $false
        if ($wtJson.profiles -and $wtJson.profiles.defaults -and $wtJson.profiles.defaults.font -and $wtJson.profiles.defaults.font.face -eq "JuliaMono") {
            $alreadySet = $true
        }

        if ($alreadySet) {
            Write-Host "Windows Terminal already configured for JuliaMono"
        } else {
            if ($y) {
                $reply = "y"
            } else {
                $reply = Read-Host "  Configure Windows Terminal to use JuliaMono? [y/N]"
            }
            if ($reply -match "^[Yy]$") {
                try {
                    if (-not $wtJson.profiles) {
                        $wtJson | Add-Member -NotePropertyName "profiles" -NotePropertyValue ([PSCustomObject]@{}) -Force
                    }
                    if (-not $wtJson.profiles.defaults) {
                        $wtJson.profiles | Add-Member -NotePropertyName "defaults" -NotePropertyValue ([PSCustomObject]@{}) -Force
                    }
                    if (-not $wtJson.profiles.defaults.font) {
                        $wtJson.profiles.defaults | Add-Member -NotePropertyName "font" -NotePropertyValue ([PSCustomObject]@{}) -Force
                    }
                    $wtJson.profiles.defaults.font | Add-Member -NotePropertyName "face" -NotePropertyValue "JuliaMono" -Force
                    $wtJson | ConvertTo-Json -Depth 10 | Set-Content $wtSettings -Encoding UTF8
                    Write-Host "  Configured: $wtSettings" -ForegroundColor Green
                    $needsRestart = $true
                } catch {
                    Write-Host "  Failed to configure Windows Terminal: $_" -ForegroundColor Red
                }
            }
        }
    }
}

Write-Host
if ($needsRestart) {
    Write-Host "NOTE: Windows Terminal should pick up the new font automatically. If it doesn't, restart it." -ForegroundColor Yellow
}
Write-Host "Restart your editor to activate Cassilda support!"
