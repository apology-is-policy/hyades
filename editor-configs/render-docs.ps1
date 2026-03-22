# Render all .cld documentation files using cassilda
# Usage: render-docs.ps1 [path-to-cassilda]

param(
    [string]$CassildaPath = ""
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$DocsDir = Join-Path $ScriptDir "docs"

if (-not $CassildaPath) {
    $CassildaPath = Join-Path $ScriptDir "bin\cassilda.exe"
}

if (-not (Test-Path $CassildaPath)) {
    Write-Host "Error: cassilda not found at $CassildaPath" -ForegroundColor Red
    Write-Host "Usage: render-docs.ps1 [path-to-cassilda]"
    exit 1
}

$count = 0
Get-ChildItem -Path $DocsDir -Filter "*.cld" -ErrorAction SilentlyContinue | ForEach-Object {
    & $CassildaPath process $_.FullName
    Write-Host "  Rendered $($_.Name)"
    $count++
}
Write-Host "Rendered $count documents in $DocsDir"
