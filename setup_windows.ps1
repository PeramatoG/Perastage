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


# Finds the latest Visual Studio installation through vswhere.
function Get-VisualStudioInstallationPath {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe was not found at '$vswhere'. Install Visual Studio with C++ desktop tools."
    }

    $installationPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $installationPath) {
        throw 'No Visual Studio installation with x64 C++ tools was found.'
    }
    return $installationPath.Trim()
}

# Imports the Visual Studio x64 developer environment into this PowerShell session.
function Initialize-X64MsvcEnvironment {
    $visualStudioPath = Get-VisualStudioInstallationPath
    $devCmd = Join-Path $visualStudioPath 'Common7\Tools\VsDevCmd.bat'
    if (-not (Test-Path $devCmd)) {
        throw "VsDevCmd.bat was not found at '$devCmd'."
    }

    Write-Host "Initializing Visual Studio x64 developer environment from: $devCmd"
    $environmentLines = cmd.exe /c "`"$devCmd`" -host_arch=x64 -arch=x64 >nul && set"
    if ($LASTEXITCODE -ne 0) {
        throw 'Failed to initialize the Visual Studio x64 developer environment.'
    }

    foreach ($line in $environmentLines) {
        $separator = $line.IndexOf('=')
        if ($separator -le 0) {
            continue
        }
        $name = $line.Substring(0, $separator)
        $value = $line.Substring($separator + 1)
        Set-Item -Path "Env:$name" -Value $value
    }

    $clCommand = Get-Command cl.exe -ErrorAction SilentlyContinue
    if (-not $clCommand) {
        throw 'cl.exe was not found after initializing the Visual Studio x64 developer environment.'
    }

    $clOutput = & cl.exe 2>&1 | Out-String
    if ($clOutput -notmatch 'x64') {
        throw "cl.exe does not appear to be the x64 compiler after initialization. Output: $clOutput"
    }

    Write-Host "MSVC compiler: $($clCommand.Source)"
    Write-Host 'MSVC compiler architecture: x64'
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
        'mdns:x64-windows',
        'gettext[tools]:x64-windows'
    )

    & $VcpkgExe install $packages

    $vcpkgRoot = Split-Path -Parent $VcpkgExe
    $msgfmt = Join-Path $vcpkgRoot 'installed\x64-windows\tools\gettext\bin\msgfmt.exe'
    if (-not (Test-Path $msgfmt)) {
        throw "vcpkg gettext msgfmt.exe was not found at: $msgfmt"
    }
    Write-Host "vcpkg msgfmt: $msgfmt"
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
Initialize-X64MsvcEnvironment

$vcpkgExePath = Assert-VcpkgAvailable -Root $VcpkgRoot
Install-PerastageDependencies -VcpkgExe $vcpkgExePath

$presets = Get-CMakePresetNames -Configuration $Configuration
Invoke-PerastageBuild `
    -ConfigurePreset $presets.Configure `
    -BuildPreset $presets.Build `
    -ShouldBuild (-not $SkipBuild)

Write-Host "Perastage Windows setup completed successfully."
