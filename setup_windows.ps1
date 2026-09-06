# Stable entry point for the Windows Ninja setup workflow.
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [string]$VcpkgRoot = '',

    [string]$VisualStudioPath = '',

    [string]$VisualStudioVersion = '',

    [string]$BashExecutable = $env:BASH_EXECUTABLE,

    [switch]$SkipBuild,

    [switch]$CleanBuild
)

$ErrorActionPreference = 'Stop'
$ImplementationScript = Join-Path $PSScriptRoot 'scripts\windows\PerastageWindowsBootstrap.ps1'
if (-not (Test-Path -LiteralPath $ImplementationScript -PathType Leaf)) {
    throw "Perastage Windows setup implementation was not found at '$ImplementationScript'."
}

& $ImplementationScript @PSBoundParameters
