# PowerShell setup script for Windows.
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [string]$VcpkgRoot = '',

    [switch]$SkipBuild,

    [switch]$CleanBuild
)

$ErrorActionPreference = 'Stop'
$PerastageVcpkgBaseline = '0878b5224d4a4968940ee296a2e7fae2d3b62983'
$PerastageVcpkgTriplet = 'x64-windows'

# Returns the repository root based on the script location.
function Get-RepositoryRoot { return Split-Path -Parent $MyInvocation.ScriptName }

# Verifies that a required command is available in PATH.
function Assert-CommandAvailable { param([Parameter(Mandatory = $true)][string]$CommandName) if (-not (Get-Command $CommandName -ErrorAction SilentlyContinue)) { throw "Required command '$CommandName' was not found in PATH." } }

# Finds the latest Visual Studio installation through vswhere.
function Get-VisualStudioInstallationPath {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { throw "vswhere.exe was not found at '$vswhere'. Install Visual Studio with C++ desktop tools." }
    $installationPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $installationPath) { throw 'No Visual Studio installation with x64 C++ tools was found.' }
    return $installationPath.Trim()
}

# Imports the Visual Studio x64 developer environment into this PowerShell session.
function Initialize-X64MsvcEnvironment {
    $visualStudioPath = Get-VisualStudioInstallationPath
    $devCmd = Join-Path $visualStudioPath 'Common7\Tools\VsDevCmd.bat'
    if (-not (Test-Path $devCmd)) { throw "VsDevCmd.bat was not found at '$devCmd'." }
    Write-Host "Initializing Visual Studio x64 developer environment from: $devCmd"
    $environmentLines = cmd.exe /c "`"$devCmd`" -host_arch=x64 -arch=x64 >nul && set"
    if ($LASTEXITCODE -ne 0) { throw 'Failed to initialize the Visual Studio x64 developer environment.' }
    foreach ($line in $environmentLines) { $separator = $line.IndexOf('='); if ($separator -le 0) { continue }; Set-Item -Path "Env:$($line.Substring(0, $separator))" -Value $line.Substring($separator + 1) }
    $clCommand = Get-Command cl.exe -ErrorAction SilentlyContinue
    if (-not $clCommand) { throw 'cl.exe was not found after initializing the Visual Studio x64 developer environment.' }
    Write-Host "MSVC compiler: $($clCommand.Source)"
    Write-Host 'MSVC compiler architecture: x64'
}

# Reads the pinned baseline from the repository manifest.
function Get-PinnedVcpkgBaseline { param([Parameter(Mandatory = $true)][string]$RepositoryRoot) return (Get-Content (Join-Path $RepositoryRoot 'vcpkg.json') -Raw | ConvertFrom-Json).'builtin-baseline' }

# Returns true when a vcpkg checkout contains local changes that must be preserved.
function Test-VcpkgCheckoutDirty { param([Parameter(Mandatory = $true)][string]$Root) $status = git -C $Root status --porcelain=v1; return [bool]$status }

# Ensures a repository-local vcpkg checkout exists unless the user supplied a root.
function Resolve-VcpkgRoot {
    param([Parameter(Mandatory = $true)][string]$RepositoryRoot, [string]$RequestedRoot)
    if (-not [string]::IsNullOrWhiteSpace($RequestedRoot)) { return (Resolve-Path -LiteralPath $RequestedRoot).Path }
    $root = Join-Path $RepositoryRoot '.tools\vcpkg'
    if (-not (Test-Path $root)) { git clone https://github.com/microsoft/vcpkg.git $root }
    return $root
}

# Checks out the pinned vcpkg baseline without destroying local work, then bootstraps vcpkg.
function Sync-AndBootstrapVcpkg {
    param([Parameter(Mandatory = $true)][string]$Root, [Parameter(Mandatory = $true)][string]$Baseline)
    if (-not (Test-Path (Join-Path $Root '.git'))) { throw "'$Root' is not a git checkout. Use a dedicated vcpkg checkout or pass -VcpkgRoot to one." }
    $current = git -C $Root rev-parse HEAD
    Write-Host "vcpkg current commit before synchronization: $current"
    Write-Host "vcpkg expected baseline: $Baseline"
    if (Test-VcpkgCheckoutDirty -Root $Root) { throw "The vcpkg checkout at '$Root' has local modifications or untracked files. Clean or stash them, or rerun with a dedicated Perastage vcpkg checkout. No checkout was changed." }
    git -C $Root fetch --depth 1 origin $Baseline
    git -C $Root checkout --detach $Baseline
    & (Join-Path $Root 'bootstrap-vcpkg.bat') -disableMetrics
    $vcpkgExe = Join-Path $Root 'vcpkg.exe'
    if (-not (Test-Path $vcpkgExe)) { throw "vcpkg.exe was not created at '$vcpkgExe'." }
    Write-Host "vcpkg final commit: $(git -C $Root rev-parse HEAD)"
    Write-Host "vcpkg executable: $vcpkgExe"
    & $vcpkgExe version
    return $vcpkgExe
}

# Installs manifest dependencies into the one explicit installed root used by CMake.
function Install-PerastageDependencies {
    param([Parameter(Mandatory = $true)][string]$VcpkgExe, [Parameter(Mandatory = $true)][string]$RepositoryRoot, [Parameter(Mandatory = $true)][string]$InstalledRoot)
    & $VcpkgExe install --triplet $script:PerastageVcpkgTriplet --x-manifest-root="$RepositoryRoot" --x-install-root="$InstalledRoot"
    $gettextBin = Join-Path $InstalledRoot "$script:PerastageVcpkgTriplet\tools\gettext\bin"
    foreach ($tool in @('msgfmt.exe', 'xgettext.exe', 'msgmerge.exe', 'msgattrib.exe')) { $toolPath = Join-Path $gettextBin $tool; if (-not (Test-Path $toolPath)) { throw "vcpkg gettext tool was not found at: $toolPath" }; Write-Host "vcpkg gettext tool: $toolPath"; & $toolPath --version | Select-Object -First 1 }
}

# Resolves the CMake configure and build presets for the selected configuration.
function Get-CMakePresetNames { param([Parameter(Mandatory = $true)][ValidateSet('Debug', 'Release')][string]$Configuration) if ($Configuration -eq 'Release') { return @{ Configure = 'win-x64-release-ninja'; Build = 'win-release-build-ninja' } }; return @{ Configure = 'win-x64-debug-ninja'; Build = 'win-debug-build-ninja' } }

# Configures and optionally builds Perastage using CMake presets.
function Invoke-PerastageBuild { param([Parameter(Mandatory = $true)][string]$ConfigurePreset, [Parameter(Mandatory = $true)][string]$BuildPreset, [Parameter(Mandatory = $true)][string]$VcpkgRoot, [Parameter(Mandatory = $true)][string]$InstalledRoot, [Parameter(Mandatory = $true)][bool]$ShouldBuild) cmake --preset $ConfigurePreset -DCMAKE_TOOLCHAIN_FILE="$VcpkgRoot\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=$script:PerastageVcpkgTriplet -DVCPKG_INSTALLED_DIR="$InstalledRoot" -DPERASTAGE_REQUIRE_SECURE_CREDENTIAL_STORE=ON; Write-Host 'Secure-store CMake probe result: passed'; if ($ShouldBuild) { cmake --build --preset $BuildPreset } }

$repoRoot = Get-RepositoryRoot
Set-Location $repoRoot
Assert-CommandAvailable -CommandName 'cmake'
Assert-CommandAvailable -CommandName 'git'
Initialize-X64MsvcEnvironment
$PerastageVcpkgBaseline = Get-PinnedVcpkgBaseline -RepositoryRoot $repoRoot
$resolvedVcpkgRoot = Resolve-VcpkgRoot -RepositoryRoot $repoRoot -RequestedRoot $VcpkgRoot
$installedRoot = Join-Path $repoRoot 'vcpkg_installed'
$presets = Get-CMakePresetNames -Configuration $Configuration
$buildDir = Join-Path $repoRoot "build\$($presets.Configure)"
if ($CleanBuild -and (Test-Path $buildDir)) { Remove-Item -LiteralPath $buildDir -Recurse -Force }
$vcpkgExePath = Sync-AndBootstrapVcpkg -Root $resolvedVcpkgRoot -Baseline $PerastageVcpkgBaseline
Write-Host "repository manifest path: $(Join-Path $repoRoot 'vcpkg.json')"
Write-Host "vcpkg installed root: $installedRoot"
Write-Host "target triplet: $PerastageVcpkgTriplet"
Install-PerastageDependencies -VcpkgExe $vcpkgExePath -RepositoryRoot $repoRoot -InstalledRoot $installedRoot
Write-Host 'Perastage dependency summary:'
Write-Host "  vcpkg root: $resolvedVcpkgRoot"
Write-Host "  pinned baseline: $PerastageVcpkgBaseline"
Write-Host "  installed root: $installedRoot"
Write-Host "  target triplet: $PerastageVcpkgTriplet"
Write-Host '  wxWidgets feature: secretstore'
Write-Host '  secure-store requirement: ON'
Write-Host '  secure-store probe: enforced during configure'
Write-Host "  build directory: build/$($presets.Configure)"
Write-Host 'Use -CleanBuild to delete only the selected Perastage build directory before reconfiguring.'
Invoke-PerastageBuild -ConfigurePreset $presets.Configure -BuildPreset $presets.Build -VcpkgRoot $resolvedVcpkgRoot -InstalledRoot $installedRoot -ShouldBuild (-not $SkipBuild)
Write-Host 'Perastage Windows setup completed successfully.'
