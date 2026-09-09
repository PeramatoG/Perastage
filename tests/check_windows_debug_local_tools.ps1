param(
    [Parameter(Mandatory = $true)]
    [string]$ModulePath
)

$ErrorActionPreference = 'Stop'
$originalPath = $env:PATH
$ripgrep = Get-Command 'rg' -ErrorAction Stop
$root = Join-Path ([System.IO.Path]::GetTempPath()) "perastage-debug-tools-$([Guid]::NewGuid())"

try {
    New-Item -ItemType Directory -Path $root -Force | Out-Null
    $env:PATH = $root
    Import-Module $ModulePath -Force

    Assert-PerastageWindowsDebugTestTools -Configuration Release
    try {
        Assert-PerastageWindowsDebugTestTools -Configuration Debug
        throw 'The Debug test-tool preflight unexpectedly passed without ripgrep.'
    } catch {
        if ($_.Exception.Message -notmatch "ripgrep \('rg'\) is required on PATH") {
            throw
        }
    }

    Copy-Item -LiteralPath $ripgrep.Source -Destination (Join-Path $root $ripgrep.Name)
    Assert-PerastageWindowsDebugTestTools -Configuration Debug
} finally {
    $env:PATH = $originalPath
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host 'OK: Windows Debug requires ripgrep while Release remains application-only.'
