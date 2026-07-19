# PowerShell setup script for validating and building the Windows Ninja workflow.
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [string]$VcpkgRoot = 'C:\vcpkg',

    [string]$VisualStudioPath = '',

    [string]$VisualStudioVersion = '',

    [switch]$SkipBuild,

    [switch]$CleanBuild
)

$ErrorActionPreference = 'Stop'
$PerastageVcpkgTriplet = 'x64-windows'

# Returns the repository root based on the script location.
function Get-RepositoryRoot {
    return Split-Path -Parent $MyInvocation.ScriptName
}

# Verifies that a required command is available in PATH.
function Assert-CommandAvailable {
    param([Parameter(Mandatory = $true)][string]$CommandName)

    if (-not (Get-Command $CommandName -ErrorAction SilentlyContinue)) {
        throw "Required command '$CommandName' was not found in PATH."
    }
}

# Prints PATH resolution diagnostics for MSVC tools.
function Write-MsvcToolDiagnostics {
    Write-Host 'where.exe cl:'
    where.exe cl 2>&1 | ForEach-Object { Write-Host "  $_" }
    Write-Host 'where.exe link:'
    where.exe link 2>&1 | ForEach-Object { Write-Host "  $_" }
}

# Normalizes a path for case-insensitive Windows prefix comparisons.
function ConvertTo-NormalizedPathText {
    param([string]$PathText)

    if ([string]::IsNullOrWhiteSpace($PathText)) {
        return ''
    }
    return $PathText.Trim('"').Replace('/', '\').TrimEnd('\').ToLowerInvariant()
}

# Finds the requested Visual Studio installation through vswhere.
function Get-VisualStudioInstallationPath {
    param(
        [string]$RequestedPath,
        [string]$RequestedVersion
    )

    if (-not [string]::IsNullOrWhiteSpace($RequestedPath)) {
        if (-not (Test-Path -LiteralPath $RequestedPath)) {
            throw "Requested Visual Studio installation path does not exist: $RequestedPath"
        }
        return (Resolve-Path -LiteralPath $RequestedPath).Path
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe was not found at '$vswhere'. Install Visual Studio with C++ desktop tools."
    }

    $arguments = @('-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath')
    if (-not [string]::IsNullOrWhiteSpace($RequestedVersion)) {
        $arguments = @('-version', $RequestedVersion, '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath')
    }

    $installationPath = & $vswhere @arguments
    if (-not $installationPath) {
        throw 'No Visual Studio installation with x64 C++ tools was found for the requested selection.'
    }
    return $installationPath.Trim()
}

# Reads the installed MSVC tools version from the Visual Studio installation when available.
function Get-MsvcToolsVersion {
    param([Parameter(Mandatory = $true)][string]$VisualStudioRoot)

    $versionFile = Join-Path $VisualStudioRoot 'VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt'
    if (Test-Path -LiteralPath $versionFile) {
        return (Get-Content -LiteralPath $versionFile -Raw).Trim()
    }
    return ''
}

# Imports and validates the Visual Studio x64 developer environment.
function Initialize-X64MsvcEnvironment {
    param(
        [string]$RequestedPath,
        [string]$RequestedVersion
    )

    $visualStudioPath = Get-VisualStudioInstallationPath -RequestedPath $RequestedPath -RequestedVersion $RequestedVersion
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
    $linkCommand = Get-Command link.exe -ErrorAction SilentlyContinue
    $hostArch = $env:VSCMD_ARG_HOST_ARCH
    $targetArch = $env:VSCMD_ARG_TGT_ARCH
    $normalizedVsRoot = ConvertTo-NormalizedPathText $visualStudioPath
    $normalizedCl = if ($clCommand) { ConvertTo-NormalizedPathText $clCommand.Source } else { '' }
    $normalizedLink = if ($linkCommand) { ConvertTo-NormalizedPathText $linkCommand.Source } else { '' }
    $validationErrors = @()

    if (-not $clCommand) { $validationErrors += 'cl.exe was not found after initializing the Visual Studio environment.' }
    if (-not $linkCommand) { $validationErrors += 'link.exe was not found after initializing the Visual Studio environment.' }
    if ($hostArch -ne 'x64') { $validationErrors += "VSCMD_ARG_HOST_ARCH is '$hostArch', expected 'x64'." }
    if ($targetArch -ne 'x64') { $validationErrors += "VSCMD_ARG_TGT_ARCH is '$targetArch', expected 'x64'." }
    if ($clCommand -and -not $normalizedCl.StartsWith($normalizedVsRoot)) { $validationErrors += "cl.exe is outside the selected Visual Studio installation: $($clCommand.Source)" }
    if ($linkCommand -and -not $normalizedLink.StartsWith($normalizedVsRoot)) { $validationErrors += "link.exe is outside the selected Visual Studio installation: $($linkCommand.Source)" }
    if ($clCommand -and $normalizedCl -notmatch 'hostx64\\x64') { $validationErrors += "cl.exe is not the preferred Hostx64\x64 tool: $($clCommand.Source)" }
    if ($linkCommand -and $normalizedLink -notmatch 'hostx64\\x64') { $validationErrors += "link.exe is not the preferred Hostx64\x64 tool: $($linkCommand.Source)" }

    $clOutput = ''
    if ($clCommand) {
        $clOutput = (& cl.exe 2>&1 | Out-String)
        if ($LASTEXITCODE -ne 0 -and $clOutput -notmatch 'Microsoft.*C/C\+\+') {
            $validationErrors += 'cl.exe did not produce the expected compiler banner.'
        }
        if ($clOutput -notmatch '(?i)for\s+x64') {
            $validationErrors += 'cl.exe banner does not identify an x64 target.'
        }
    }

    if ($validationErrors.Count -gt 0) {
        Write-Host 'MSVC x64 environment validation failed:'
        $validationErrors | ForEach-Object { Write-Host "  $_" }
        Write-MsvcToolDiagnostics
        throw 'Visual Studio x64 compiler environment is invalid; refusing to configure an x64 Ninja build with mixed tools.'
    }

    $toolsVersion = Get-MsvcToolsVersion -VisualStudioRoot $visualStudioPath
    Write-Host "Visual Studio installation: $visualStudioPath"
    Write-Host "MSVC tools version: $toolsVersion"
    Write-Host "MSVC compiler: $($clCommand.Source)"
    Write-Host "MSVC linker: $($linkCommand.Source)"
    Write-Host "MSVC host architecture: $hostArch"
    Write-Host "MSVC target architecture: $targetArch"

    return [pscustomobject]@{
        VisualStudioRoot = $visualStudioPath
        CompilerPath = $clCommand.Source
        LinkerPath = $linkCommand.Source
        ToolsVersion = $toolsVersion
        HostArchitecture = $hostArch
        TargetArchitecture = $targetArch
    }
}

# Resolves and validates the selected classic vcpkg installation.
function Resolve-ClassicVcpkgInstallation {
    param([Parameter(Mandatory = $true)][string]$Root)

    if (-not (Test-Path -LiteralPath $Root)) {
        throw "The vcpkg root does not exist: $Root. Install dependencies once into C:\vcpkg or pass -VcpkgRoot to an existing classic vcpkg checkout."
    }

    $resolvedRoot = (Resolve-Path -LiteralPath $Root).Path
    $vcpkgExe = Join-Path $resolvedRoot 'vcpkg.exe'
    $toolchainFile = Join-Path $resolvedRoot 'scripts\buildsystems\vcpkg.cmake'
    $installedTriplet = Join-Path $resolvedRoot "installed\$script:PerastageVcpkgTriplet"
    $errors = @()

    if (-not (Test-Path -LiteralPath $vcpkgExe)) { $errors += "Missing vcpkg executable: $vcpkgExe" }
    if (-not (Test-Path -LiteralPath $toolchainFile)) { $errors += "Missing vcpkg CMake toolchain: $toolchainFile" }
    if (-not (Test-Path -LiteralPath $installedTriplet)) { $errors += "Missing installed triplet directory: $installedTriplet" }

    if ($errors.Count -gt 0) {
        $errors | ForEach-Object { Write-Host "  $_" }
        throw "The selected vcpkg root is not ready for the Perastage classic Windows workflow. Install the required x64-windows dependencies manually in '$resolvedRoot'; setup_windows.ps1 does not modify vcpkg."
    }

    Write-Host "vcpkg root: $resolvedRoot"
    Write-Host "vcpkg executable: $vcpkgExe"
    & $vcpkgExe version
    return [pscustomobject]@{
        Root = $resolvedRoot
        Executable = $vcpkgExe
        ToolchainFile = $toolchainFile
        InstalledTriplet = $installedTriplet
    }
}

# Validates representative headers, libraries, and build tools from the classic vcpkg triplet.
function Test-PerastageVcpkgDependencies {
    param([Parameter(Mandatory = $true)][object]$Vcpkg)

    $checks = @(
        @{ Name = 'wxWidgets'; Path = 'include\wx\secretstore.h' },
        @{ Name = 'CURL'; Path = 'include\curl\curl.h' },
        @{ Name = 'tinyxml2'; Path = 'include\tinyxml2.h' },
        @{ Name = 'ZLIB'; Path = 'include\zlib.h' },
        @{ Name = 'GLEW'; Path = 'include\GL\glew.h' },
        @{ Name = 'NanoVG'; Path = 'include\nanovg.h' },
        @{ Name = 'PoDoFo'; Path = 'include\podofo\podofo.h' },
        @{ Name = 'meshoptimizer'; Path = 'include\meshoptimizer.h' },
        @{ Name = 'Backward'; Path = 'include\backward.hpp' },
        @{ Name = 'mdns'; Path = 'include\mdns.h' },
        @{ Name = 'gettext msgfmt'; Path = 'tools\gettext\bin\msgfmt.exe' },
        @{ Name = 'gettext xgettext'; Path = 'tools\gettext\bin\xgettext.exe' },
        @{ Name = 'gettext msgmerge'; Path = 'tools\gettext\bin\msgmerge.exe' },
        @{ Name = 'gettext msgattrib'; Path = 'tools\gettext\bin\msgattrib.exe' }
    )

    $missing = @()
    foreach ($check in $checks) {
        $path = Join-Path $Vcpkg.InstalledTriplet $check.Path
        if (-not (Test-Path -LiteralPath $path)) {
            $missing += "$($check.Name): $path"
        }
    }

    if ($missing.Count -gt 0) {
        Write-Host 'Missing required vcpkg files:'
        $missing | ForEach-Object { Write-Host "  $_" }
        throw "Install the missing x64-windows packages into '$($Vcpkg.Root)' before configuring. The setup script validates dependencies but never installs or rebuilds them."
    }

    $setupCandidates = @(
        (Join-Path $Vcpkg.InstalledTriplet 'include\wx\msw\setup.h'),
        (Join-Path $Vcpkg.InstalledTriplet 'include\wx\setup.h')
    )
    $setupHeader = $setupCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if (-not $setupHeader) {
        throw "Unable to find wxWidgets setup.h under '$($Vcpkg.InstalledTriplet)'."
    }
    if ((Get-Content -LiteralPath $setupHeader -Raw) -notmatch '#\s*define\s+wxUSE_SECRETSTORE\s+1') {
        throw "wxWidgets was found, but wxUSE_SECRETSTORE is not enabled in '$setupHeader'. Rebuild wxWidgets with the secretstore feature in the selected vcpkg installation."
    }

    $gettextBin = Join-Path $Vcpkg.InstalledTriplet 'tools\gettext\bin'
    foreach ($tool in @('msgfmt.exe', 'xgettext.exe', 'msgmerge.exe', 'msgattrib.exe')) {
        $toolPath = Join-Path $gettextBin $tool
        Write-Host "gettext tool: $toolPath"
        & $toolPath --version | Select-Object -First 1
    }
}

# Resolves the CMake configure and build presets for the selected configuration.
function Get-CMakePresetNames {
    param([Parameter(Mandatory = $true)][ValidateSet('Debug', 'Release')][string]$Configuration)

    if ($Configuration -eq 'Release') {
        return @{ Configure = 'win-x64-release-ninja'; Build = 'win-release-build-ninja' }
    }
    return @{ Configure = 'win-x64-debug-ninja'; Build = 'win-debug-build-ninja' }
}

# Returns cache entries from a CMakeCache.txt file.
function Read-CMakeCacheEntries {
    param([Parameter(Mandatory = $true)][string]$CachePath)

    $entries = @{}
    foreach ($line in Get-Content -LiteralPath $CachePath) {
        if ($line -match '^([^#/][^:]*):[^=]*=(.*)$') {
            $entries[$matches[1]] = $matches[2]
        }
    }
    return $entries
}

# Returns the Visual Studio installation root inferred from an MSVC compiler path.
function Get-VisualStudioRootFromCompilerPath {
    param([string]$CompilerPath)

    if ([string]::IsNullOrWhiteSpace($CompilerPath)) {
        return ''
    }
    $normalized = $CompilerPath.Replace('/', '\')
    $marker = '\VC\Tools\MSVC\'
    $index = $normalized.IndexOf($marker, [StringComparison]::OrdinalIgnoreCase)
    if ($index -lt 0) {
        return ''
    }
    return $normalized.Substring(0, $index)
}

# Removes the selected build directory when an old cache cannot produce a valid x64 classic-vcpkg build.
function Reset-IncompatibleCMakeCache {
    param(
        [Parameter(Mandatory = $true)][string]$BuildDirectory,
        [Parameter(Mandatory = $true)][object]$MsvcEnvironment,
        [Parameter(Mandatory = $true)][string]$ExpectedToolchainFile,
        [Parameter(Mandatory = $true)][string]$ExpectedTriplet,
        [Parameter(Mandatory = $true)][bool]$ForceClean
    )

    if ($ForceClean) {
        if (Test-Path -LiteralPath $BuildDirectory) {
            Write-Host "-CleanBuild selected; removing build directory: $BuildDirectory"
            Remove-Item -LiteralPath $BuildDirectory -Recurse -Force
        }
        return
    }

    $cachePath = Join-Path $BuildDirectory 'CMakeCache.txt'
    if (-not (Test-Path -LiteralPath $cachePath)) {
        return
    }

    $entries = Read-CMakeCacheEntries -CachePath $cachePath
    $cachedCxx = $entries['CMAKE_CXX_COMPILER']
    $cachedC = $entries['CMAKE_C_COMPILER']
    $cachedGenerator = $entries['CMAKE_GENERATOR']
    $cachedToolchain = $entries['CMAKE_TOOLCHAIN_FILE']
    $cachedTriplet = $entries['VCPKG_TARGET_TRIPLET']
    $cachedManifestMode = $entries['VCPKG_MANIFEST_MODE']
    $cachedManifestInstall = $entries['VCPKG_MANIFEST_INSTALL']
    $cachedInstalledDir = $entries['VCPKG_INSTALLED_DIR']
    $cachedVsRoot = Get-VisualStudioRootFromCompilerPath -CompilerPath $cachedCxx
    $expectedVsRoot = $MsvcEnvironment.VisualStudioRoot
    $reasons = @()

    if ($cachedGenerator -and $cachedGenerator -ne 'Ninja') { $reasons += "cached generator is '$cachedGenerator', expected 'Ninja'" }
    if ($cachedTriplet -and $cachedTriplet -ne $ExpectedTriplet) { $reasons += "cached vcpkg triplet is '$cachedTriplet', expected '$ExpectedTriplet'" }
    if ($cachedToolchain -and (ConvertTo-NormalizedPathText $cachedToolchain) -ne (ConvertTo-NormalizedPathText $ExpectedToolchainFile)) { $reasons += "cached toolchain '$cachedToolchain' differs from requested '$ExpectedToolchainFile'" }
    if ($cachedManifestMode -and $cachedManifestMode -ne 'OFF') { $reasons += "cached VCPKG_MANIFEST_MODE is '$cachedManifestMode', expected 'OFF'" }
    if ($cachedManifestInstall -and $cachedManifestInstall -ne 'OFF') { $reasons += "cached VCPKG_MANIFEST_INSTALL is '$cachedManifestInstall', expected 'OFF'" }
    if ($cachedInstalledDir) { $reasons += "cached VCPKG_INSTALLED_DIR is '$cachedInstalledDir', but the local classic workflow must use the selected vcpkg root" }
    if ($cachedCxx -and (ConvertTo-NormalizedPathText $cachedCxx) -match 'hostx86\\x86|\\x86\\cl\.exe$') { $reasons += "cached C++ compiler targets x86: $cachedCxx" }
    if ($cachedC -and (ConvertTo-NormalizedPathText $cachedC) -match 'hostx86\\x86|\\x86\\cl\.exe$') { $reasons += "cached C compiler targets x86: $cachedC" }
    if ($cachedVsRoot -and (ConvertTo-NormalizedPathText $cachedVsRoot) -ne (ConvertTo-NormalizedPathText $expectedVsRoot)) { $reasons += "cached compiler Visual Studio root '$cachedVsRoot' differs from selected '$expectedVsRoot'" }

    if ($reasons.Count -gt 0) {
        Write-Host 'Detected incompatible CMake cache for requested Windows x64 Ninja build:'
        $reasons | ForEach-Object { Write-Host "  $_" }
        Write-Host "Cached CMAKE_CXX_COMPILER: $cachedCxx"
        Write-Host "Cached CMAKE_C_COMPILER: $cachedC"
        Write-Host "Selected compiler: $($MsvcEnvironment.CompilerPath)"
        Write-Host "Selected linker: $($MsvcEnvironment.LinkerPath)"
        Write-Host "Removing only selected build directory: $BuildDirectory"
        Remove-Item -LiteralPath $BuildDirectory -Recurse -Force
    }
}

# Configures and optionally builds Perastage using the shared Windows Ninja presets.
function Invoke-PerastageBuild {
    param(
        [Parameter(Mandatory = $true)][string]$ConfigurePreset,
        [Parameter(Mandatory = $true)][string]$BuildPreset,
        [Parameter(Mandatory = $true)][bool]$ShouldBuild
    )

    cmake --preset $ConfigurePreset `
        -DPERASTAGE_REQUIRE_SECURE_CREDENTIAL_STORE=ON `
        -DVCPKG_MANIFEST_MODE=OFF `
        -DVCPKG_MANIFEST_INSTALL=OFF
    Write-Host 'Secure-store CMake probe result: passed'

    if ($ShouldBuild) {
        cmake --build --preset $BuildPreset
    }
}

$repoRoot = Get-RepositoryRoot
Set-Location $repoRoot
Assert-CommandAvailable -CommandName 'cmake'
$msvcEnvironment = Initialize-X64MsvcEnvironment -RequestedPath $VisualStudioPath -RequestedVersion $VisualStudioVersion
$resolvedVcpkg = Resolve-ClassicVcpkgInstallation -Root $VcpkgRoot
Test-PerastageVcpkgDependencies -Vcpkg $resolvedVcpkg

$presets = Get-CMakePresetNames -Configuration $Configuration
$buildDir = Join-Path $repoRoot "build\$($presets.Configure)"
Reset-IncompatibleCMakeCache `
    -BuildDirectory $buildDir `
    -MsvcEnvironment $msvcEnvironment `
    -ExpectedToolchainFile $resolvedVcpkg.ToolchainFile `
    -ExpectedTriplet $PerastageVcpkgTriplet `
    -ForceClean ([bool]$CleanBuild)

$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
$ninjaCommand = Get-Command ninja -ErrorAction SilentlyContinue
Write-Host 'Perastage Windows classic-vcpkg summary:'
Write-Host "  Visual Studio root: $($msvcEnvironment.VisualStudioRoot)"
Write-Host "  MSVC compiler: $($msvcEnvironment.CompilerPath)"
Write-Host "  MSVC linker: $($msvcEnvironment.LinkerPath)"
Write-Host "  MSVC host architecture: $($msvcEnvironment.HostArchitecture)"
Write-Host "  MSVC target architecture: $($msvcEnvironment.TargetArchitecture)"
Write-Host "  MSVC tools version: $($msvcEnvironment.ToolsVersion)"
Write-Host "  CMake executable: $($cmakeCommand.Source)"
cmake --version | Select-Object -First 1
if ($ninjaCommand) {
    Write-Host "  Ninja executable: $($ninjaCommand.Source)"
    ninja --version | Select-Object -First 1
} else {
    Write-Host '  Ninja executable: not found in PATH before configure'
}
Write-Host "  vcpkg root: $($resolvedVcpkg.Root)"
Write-Host "  vcpkg installed triplet: $($resolvedVcpkg.InstalledTriplet)"
Write-Host "  target triplet: $PerastageVcpkgTriplet"
Write-Host '  wxWidgets feature: secretstore validated in classic vcpkg installation'
Write-Host '  secure-store requirement: ON'
Write-Host '  CMake vcpkg manifest mode: OFF'
Write-Host '  CMake vcpkg manifest install: OFF'
Write-Host "  configure preset: $($presets.Configure)"
Write-Host "  build preset: $($presets.Build)"
Write-Host "  binary directory: $buildDir"
Write-Host 'Use -CleanBuild to delete only the selected Perastage build directory before reconfiguring.'
Invoke-PerastageBuild `
    -ConfigurePreset $presets.Configure `
    -BuildPreset $presets.Build `
    -ShouldBuild (-not $SkipBuild)
Write-Host 'Perastage Windows setup validation completed successfully.'
