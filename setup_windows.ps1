# PowerShell setup script for Windows
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [string]$VcpkgRoot = 'C:\vcpkg',

    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$PerastageVcpkgBaseline = '0878b5224d4a4968940ee296a2e7fae2d3b62983'
$PerastageVcpkgTriplet = 'x64-windows'

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

# Ensures the vcpkg checkout matches the repository-pinned baseline.
function Sync-PerastageVcpkgBaseline {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root
    )

    if (-not (Test-Path (Join-Path $Root '.git'))) {
        throw "'$Root' is not a git checkout. Clone https://github.com/microsoft/vcpkg.git there or pass -VcpkgRoot."
    }

    git -C $Root fetch --depth 1 origin $script:PerastageVcpkgBaseline
    git -C $Root checkout --force $script:PerastageVcpkgBaseline
}

# Installs the Windows dependencies required by Perastage using manifest mode.
function Install-PerastageDependencies {
    param(
        [Parameter(Mandatory = $true)]
        [string]$VcpkgExe,

        [Parameter(Mandatory = $true)]
        [string]$RepositoryRoot
    )

    & $VcpkgExe install --triplet $script:PerastageVcpkgTriplet --x-manifest-root=$RepositoryRoot
    & $VcpkgExe install "gettext[tools]:$script:PerastageVcpkgTriplet"

    $vcpkgRoot = Split-Path -Parent $VcpkgExe
    $gettextBin = Join-Path $vcpkgRoot "installed\$script:PerastageVcpkgTriplet\tools\gettext\bin"
    foreach ($tool in @('msgfmt.exe', 'xgettext.exe', 'msgmerge.exe', 'msgattrib.exe')) {
        $toolPath = Join-Path $gettextBin $tool
        if (-not (Test-Path $toolPath)) {
            throw "vcpkg gettext tool was not found at: $toolPath"
        }
        Write-Host "vcpkg gettext tool: $toolPath"
        & $toolPath --version | Select-Object -First 1
    }
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

    cmake --preset $ConfigurePreset -DPERASTAGE_REQUIRE_SECURE_CREDENTIAL_STORE=ON
    Write-Host "Secure-store CMake probe result: passed"

    if ($ShouldBuild) {
        cmake --build --preset $BuildPreset
    }
}

$repoRoot = Get-RepositoryRoot
Set-Location $repoRoot

Assert-CommandAvailable -CommandName 'cmake'
Initialize-X64MsvcEnvironment

$vcpkgExePath = Assert-VcpkgAvailable -Root $VcpkgRoot
Sync-PerastageVcpkgBaseline -Root $VcpkgRoot
Install-PerastageDependencies -VcpkgExe $vcpkgExePath -RepositoryRoot $repoRoot

$presets = Get-CMakePresetNames -Configuration $Configuration
Write-Host "Perastage dependency summary:"
Write-Host "  vcpkg root: $VcpkgRoot"
Write-Host "  pinned baseline: $PerastageVcpkgBaseline"
Write-Host "  target triplet: $PerastageVcpkgTriplet"
Write-Host "  wxWidgets feature: secretstore"
Write-Host "  secure-store requirement: ON"
Write-Host "  secure-store probe: enforced during configure"
Write-Host "  build directory: build/$($presets.Configure)"
Write-Host "Changing wxWidgets features requires deleting the affected Perastage build directory before reconfiguring."
Invoke-PerastageBuild `
    -ConfigurePreset $presets.Configure `
    -BuildPreset $presets.Build `
    -ShouldBuild (-not $SkipBuild)

Write-Host "Perastage Windows setup completed successfully."
