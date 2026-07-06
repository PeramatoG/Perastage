# PowerShell setup script for Windows
$ErrorActionPreference = 'Stop'

param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [string]$VcpkgRoot = 'C:\vcpkg',

    [switch]$SkipBuild
)

# Returns the repository root based on the script location.
function Get-RepositoryRoot {
    return Split-Path -Parent $MyInvocation.ScriptName
}

# Verifies that a required command is available in PATH.
function Assert-CommandAvailable {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CommandName
    )

    if (-not (Get-Command $CommandName -ErrorAction SilentlyContinue)) {
        throw "Required command '$CommandName' was not found in PATH."
    }
}

# Verifies that the expected vcpkg installation exists.
function Assert-VcpkgAvailable {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root
    )

    $vcpkgExe = Join-Path $Root 'vcpkg.exe'
    $toolchainFile = Join-Path $Root 'scripts\buildsystems\vcpkg.cmake'

    if (-not (Test-Path $vcpkgExe)) {
        throw "vcpkg.exe was not found at '$vcpkgExe'. Install vcpkg in '$Root' or pass -VcpkgRoot with the correct path."
    }

    if (-not (Test-Path $toolchainFile)) {
        throw "vcpkg CMake toolchain file was not found at '$toolchainFile'."
    }

    return $vcpkgExe
}

# Installs the Windows dependencies required by Perastage.
function Install-PerastageDependencies {
    param(
        [Parameter(Mandatory = $true)]
        [string]$VcpkgExe
    )

    $packages = @(
        'wxwidgets:x64-windows',
        'tinyxml2:x64-windows',
        'curl:x64-windows',
        'glew:x64-windows',
        'meshoptimizer:x64-windows',
        'nanovg:x64-windows',
        'podofo:x64-windows',
        'zlib:x64-windows',
        'backward-cpp:x64-windows',
        'mdns:x64-windows'
    )

    & $VcpkgExe install $packages
}

# Resolves the CMake configure and build presets for the selected configuration.
function Get-CMakePresetNames {
    param(
        [Parameter(Mandatory = $true)]
        [ValidateSet('Debug', 'Release')]
        [string]$Configuration
    )

    if ($Configuration -eq 'Release') {
        return @{
            Configure = 'win-x64-release-ninja'
            Build = 'win-release-build-ninja'
        }
    }

    return @{
        Configure = 'win-x64-debug-ninja'
        Build = 'win-debug-build-ninja'
    }
}

# Configures and optionally builds Perastage using CMake presets.
function Invoke-PerastageBuild {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ConfigurePreset,

        [Parameter(Mandatory = $true)]
        [string]$BuildPreset,

        [Parameter(Mandatory = $true)]
        [bool]$ShouldBuild
    )

    cmake --preset $ConfigurePreset

    if ($ShouldBuild) {
        cmake --build --preset $BuildPreset
    }
}

$repoRoot = Get-RepositoryRoot
Set-Location $repoRoot

Assert-CommandAvailable -CommandName 'cmake'

$vcpkgExePath = Assert-VcpkgAvailable -Root $VcpkgRoot
Install-PerastageDependencies -VcpkgExe $vcpkgExePath

$presets = Get-CMakePresetNames -Configuration $Configuration
Invoke-PerastageBuild `
    -ConfigurePreset $presets.Configure `
    -BuildPreset $presets.Build `
    -ShouldBuild (-not $SkipBuild)

Write-Host "Perastage Windows setup completed successfully."
