# PowerShell setup script for Windows.
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [string]$VcpkgRoot = '',

    [string]$VisualStudioPath = '',

    [string]$VisualStudioVersion = '',

    [switch]$SkipBuild,

    [switch]$CleanBuild
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
    $normalizedCl = ConvertTo-NormalizedPathText $clCommand.Source
    $normalizedLink = ConvertTo-NormalizedPathText $linkCommand.Source
    $validationErrors = @()

    if (-not $clCommand) { $validationErrors += 'cl.exe was not found after initializing the Visual Studio environment.' }
    if (-not $linkCommand) { $validationErrors += 'link.exe was not found after initializing the Visual Studio environment.' }
    if ($hostArch -ne 'x64') { $validationErrors += "VSCMD_ARG_HOST_ARCH is '$hostArch', expected 'x64'." }
    if ($targetArch -ne 'x64') { $validationErrors += "VSCMD_ARG_TGT_ARCH is '$targetArch', expected 'x64'." }
    if ($clCommand -and -not $normalizedCl.StartsWith($normalizedVsRoot)) { $validationErrors += "cl.exe is outside the selected Visual Studio installation: $($clCommand.Source)" }
    if ($linkCommand -and -not $normalizedLink.StartsWith($normalizedVsRoot)) { $validationErrors += "link.exe is outside the selected Visual Studio installation: $($linkCommand.Source)" }
    if ($clCommand -and $normalizedCl -notmatch 'hostx64\\x64') { $validationErrors += "cl.exe is not the preferred Hostx64\\x64 tool: $($clCommand.Source)" }
    if ($linkCommand -and $normalizedLink -notmatch 'hostx64\\x64') { $validationErrors += "link.exe is not the preferred Hostx64\\x64 tool: $($linkCommand.Source)" }

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

# Reads the pinned baseline from the repository manifest.
function Get-PinnedVcpkgBaseline {
    param([Parameter(Mandatory = $true)][string]$RepositoryRoot)

    return (Get-Content (Join-Path $RepositoryRoot 'vcpkg.json') -Raw | ConvertFrom-Json).'builtin-baseline'
}

# Returns true when a vcpkg checkout contains local changes that must be preserved.
function Test-VcpkgCheckoutDirty {
    param([Parameter(Mandatory = $true)][string]$Root)

    $status = git -C $Root status --porcelain=v1
    return [bool]$status
}

# Ensures a repository-local vcpkg checkout exists unless the user supplied a root.
function Resolve-VcpkgRoot {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [string]$RequestedRoot
    )

    if (-not [string]::IsNullOrWhiteSpace($RequestedRoot)) {
        if (-not (Test-Path -LiteralPath $RequestedRoot)) {
            throw "The requested vcpkg root does not exist: $RequestedRoot"
        }
        return (Resolve-Path -LiteralPath $RequestedRoot).Path
    }

    $root = Join-Path $RepositoryRoot '.tools\vcpkg'
    if (-not (Test-Path $root)) {
        git clone https://github.com/microsoft/vcpkg.git $root
    }
    return $root
}

# Checks out the pinned vcpkg baseline without destroying local work, then bootstraps vcpkg.
function Sync-AndBootstrapVcpkg {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$Baseline
    )

    if (-not (Test-Path (Join-Path $Root '.git'))) {
        throw "'$Root' is not a git checkout. Use a dedicated vcpkg checkout or pass -VcpkgRoot to one."
    }

    $current = git -C $Root rev-parse HEAD
    Write-Host "vcpkg current commit before synchronization: $current"
    Write-Host "vcpkg expected baseline: $Baseline"

    if (Test-VcpkgCheckoutDirty -Root $Root) {
        throw "The vcpkg checkout at '$Root' has local modifications or untracked files. Clean or stash them, or rerun with the default Perastage-local checkout. No checkout was changed."
    }

    git -C $Root fetch --depth 1 origin $Baseline
    git -C $Root checkout --detach $Baseline

    $bootstrap = Join-Path $Root 'bootstrap-vcpkg.bat'
    & $bootstrap -disableMetrics

    $vcpkgExe = Join-Path $Root 'vcpkg.exe'
    if (-not (Test-Path $vcpkgExe)) {
        throw "vcpkg.exe was not created at '$vcpkgExe'."
    }

    Write-Host "vcpkg final commit: $(git -C $Root rev-parse HEAD)"
    Write-Host "vcpkg executable: $vcpkgExe"
    & $vcpkgExe version
    return $vcpkgExe
}

# Installs manifest dependencies into the one explicit installed root used by CMake.
function Install-PerastageDependencies {
    param(
        [Parameter(Mandatory = $true)][string]$VcpkgExe,
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][string]$InstalledRoot
    )

    & $VcpkgExe install `
        --triplet $script:PerastageVcpkgTriplet `
        --x-manifest-root="$RepositoryRoot" `
        --x-install-root="$InstalledRoot"

    $gettextBin = Join-Path $InstalledRoot "$script:PerastageVcpkgTriplet\tools\gettext\bin"
    foreach ($tool in @('msgfmt.exe', 'xgettext.exe', 'msgmerge.exe', 'msgattrib.exe')) {
        $toolPath = Join-Path $gettextBin $tool
        if (-not (Test-Path $toolPath)) {
            throw "vcpkg gettext tool was not found at: $toolPath"
        }
        Write-Host "vcpkg gettext tool: $toolPath"
        & $toolPath --version | Select-Object -First 1
    }
}

# Returns true when a CMake user preset document contains the required local presets.
function Test-PerastageLocalPresetsCompatible {
    param(
        [Parameter(Mandatory = $true)][string]$PresetPath,
        [Parameter(Mandatory = $true)][string]$ExpectedVcpkgRoot,
        [Parameter(Mandatory = $true)][string]$ExpectedInstalledRoot
    )

    $document = Get-Content -LiteralPath $PresetPath -Raw | ConvertFrom-Json
    $buildNames = @($document.buildPresets | ForEach-Object { $_.name })
    if ($buildNames -notcontains 'local-win-debug-build-ninja' -or $buildNames -notcontains 'local-win-release-build-ninja') {
        return $false
    }

    $expectedToolchain = (Join-Path $ExpectedVcpkgRoot 'scripts\buildsystems\vcpkg.cmake').Replace('\', '/')
    $expectedInstalled = $ExpectedInstalledRoot.Replace('\', '/')
    foreach ($name in @('local-win-x64-debug-ninja', 'local-win-x64-release-ninja')) {
        $preset = @($document.configurePresets | Where-Object { $_.name -eq $name }) | Select-Object -First 1
        if (-not $preset) { return $false }
        if ($preset.cacheVariables.CMAKE_TOOLCHAIN_FILE -ne $expectedToolchain) { return $false }
        if ($preset.cacheVariables.VCPKG_INSTALLED_DIR -ne $expectedInstalled) { return $false }
        if ($preset.cacheVariables.VCPKG_MANIFEST_MODE -ne 'OFF') { return $false }
    }
    return $true
}

# Writes local CMake presets for the selected vcpkg checkout without changing shared presets.
function Write-PerastageCMakeUserPresets {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][string]$VcpkgRoot,
        [Parameter(Mandatory = $true)][string]$InstalledRoot
    )

    $userPresetsPath = Join-Path $RepositoryRoot 'CMakeUserPresets.json'
    if (Test-Path -LiteralPath $userPresetsPath) {
        if (-not (Test-PerastageLocalPresetsCompatible -PresetPath $userPresetsPath -ExpectedVcpkgRoot $VcpkgRoot -ExpectedInstalledRoot $InstalledRoot)) {
            throw "CMakeUserPresets.json exists but does not define compatible Perastage local Windows presets. Add local-win-x64-debug-ninja/local-win-x64-release-ninja entries pointing to '$VcpkgRoot' and '$InstalledRoot', or move the file aside so setup_windows.ps1 can generate them. The file was not modified."
        }
        Write-Host 'CMakeUserPresets.json already contains Perastage local presets; leaving user content unchanged.'
        return
    }

    $toolchainPath = (Join-Path $VcpkgRoot 'scripts\buildsystems\vcpkg.cmake').Replace('\', '/')
    $installedPath = $InstalledRoot.Replace('\', '/')
    $presetDocument = [ordered]@{
        version = 3
        configurePresets = @(
            [ordered]@{
                name = 'local-win-x64-debug-ninja'
                displayName = 'Local Windows x64 Debug (Ninja)'
                inherits = 'win-x64-debug-ninja'
                architecture = [ordered]@{ value = 'x64'; strategy = 'external' }
                cacheVariables = [ordered]@{
                    CMAKE_TOOLCHAIN_FILE = $toolchainPath
                    VCPKG_INSTALLED_DIR = $installedPath
                    VCPKG_MANIFEST_MODE = 'OFF'
                }
            },
            [ordered]@{
                name = 'local-win-x64-release-ninja'
                displayName = 'Local Windows x64 Release (Ninja)'
                inherits = 'win-x64-release-ninja'
                architecture = [ordered]@{ value = 'x64'; strategy = 'external' }
                cacheVariables = [ordered]@{
                    CMAKE_TOOLCHAIN_FILE = $toolchainPath
                    VCPKG_INSTALLED_DIR = $installedPath
                    VCPKG_MANIFEST_MODE = 'OFF'
                }
            }
        )
        buildPresets = @(
            [ordered]@{
                name = 'local-win-debug-build-ninja'
                displayName = 'Local Build Windows Debug (Ninja)'
                configurePreset = 'local-win-x64-debug-ninja'
                jobs = 8
            },
            [ordered]@{
                name = 'local-win-release-build-ninja'
                displayName = 'Local Build Windows Release (Ninja)'
                configurePreset = 'local-win-x64-release-ninja'
                jobs = 8
            }
        )
    }

    $presetDocument | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $userPresetsPath -Encoding UTF8
    Write-Host "Wrote local CMake presets: $userPresetsPath"
}

# Resolves the CMake configure and build presets for the selected configuration.
function Get-CMakePresetNames {
    param([Parameter(Mandatory = $true)][ValidateSet('Debug', 'Release')][string]$Configuration)

    if ($Configuration -eq 'Release') {
        return @{ Configure = 'local-win-x64-release-ninja'; Build = 'local-win-release-build-ninja' }
    }
    return @{ Configure = 'local-win-x64-debug-ninja'; Build = 'local-win-debug-build-ninja' }
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

# Removes the selected build directory when an old cache cannot produce a valid x64 build.
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
    $cachedVsRoot = Get-VisualStudioRootFromCompilerPath -CompilerPath $cachedCxx
    $expectedVsRoot = $MsvcEnvironment.VisualStudioRoot
    $reasons = @()

    if ($cachedGenerator -and $cachedGenerator -ne 'Ninja') { $reasons += "cached generator is '$cachedGenerator', expected 'Ninja'" }
    if ($cachedTriplet -and $cachedTriplet -ne $ExpectedTriplet) { $reasons += "cached vcpkg triplet is '$cachedTriplet', expected '$ExpectedTriplet'" }
    if ($cachedToolchain -and (ConvertTo-NormalizedPathText $cachedToolchain) -ne (ConvertTo-NormalizedPathText $ExpectedToolchainFile)) { $reasons += "cached toolchain '$cachedToolchain' differs from requested '$ExpectedToolchainFile'" }
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

# Configures and optionally builds Perastage using generated local CMake presets.
function Invoke-PerastageBuild {
    param(
        [Parameter(Mandatory = $true)][string]$ConfigurePreset,
        [Parameter(Mandatory = $true)][string]$BuildPreset,
        [Parameter(Mandatory = $true)][bool]$ShouldBuild
    )

    cmake --preset $ConfigurePreset `
        -DPERASTAGE_REQUIRE_SECURE_CREDENTIAL_STORE=ON
    Write-Host 'Secure-store CMake probe result: passed'

    if ($ShouldBuild) {
        cmake --build --preset $BuildPreset
    }
}

$repoRoot = Get-RepositoryRoot
Set-Location $repoRoot
Assert-CommandAvailable -CommandName 'cmake'
Assert-CommandAvailable -CommandName 'git'
$msvcEnvironment = Initialize-X64MsvcEnvironment -RequestedPath $VisualStudioPath -RequestedVersion $VisualStudioVersion

$PerastageVcpkgBaseline = Get-PinnedVcpkgBaseline -RepositoryRoot $repoRoot
$resolvedVcpkgRoot = Resolve-VcpkgRoot -RepositoryRoot $repoRoot -RequestedRoot $VcpkgRoot
$installedRoot = Join-Path $repoRoot 'vcpkg_installed'
$vcpkgToolchainFile = Join-Path $resolvedVcpkgRoot 'scripts\buildsystems\vcpkg.cmake'
$vcpkgExePath = Sync-AndBootstrapVcpkg -Root $resolvedVcpkgRoot -Baseline $PerastageVcpkgBaseline
Write-Host "repository manifest path: $(Join-Path $repoRoot 'vcpkg.json')"
Write-Host "vcpkg installed root: $installedRoot"
Write-Host "target triplet: $PerastageVcpkgTriplet"
Install-PerastageDependencies -VcpkgExe $vcpkgExePath -RepositoryRoot $repoRoot -InstalledRoot $installedRoot
Write-PerastageCMakeUserPresets -RepositoryRoot $repoRoot -VcpkgRoot $resolvedVcpkgRoot -InstalledRoot $installedRoot

$presets = Get-CMakePresetNames -Configuration $Configuration
$buildDir = Join-Path $repoRoot "build\$($presets.Configure)"
Reset-IncompatibleCMakeCache `
    -BuildDirectory $buildDir `
    -MsvcEnvironment $msvcEnvironment `
    -ExpectedToolchainFile $vcpkgToolchainFile `
    -ExpectedTriplet $PerastageVcpkgTriplet `
    -ForceClean ([bool]$CleanBuild)

$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
$ninjaCommand = Get-Command ninja -ErrorAction SilentlyContinue
Write-Host 'Perastage dependency summary:'
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
Write-Host "  vcpkg root: $resolvedVcpkgRoot"
Write-Host "  pinned baseline: $PerastageVcpkgBaseline"
Write-Host "  installed root: $installedRoot"
Write-Host "  target triplet: $PerastageVcpkgTriplet"
Write-Host '  wxWidgets feature: secretstore'
Write-Host '  secure-store requirement: ON'
Write-Host '  secure-store probe: enforced during configure'
Write-Host '  CMake vcpkg manifest mode: OFF after explicit manifest install'
Write-Host "  configure preset: $($presets.Configure)"
Write-Host "  build preset: $($presets.Build)"
Write-Host "  binary directory: $buildDir"
Write-Host 'Use -CleanBuild to delete only the selected Perastage build directory before reconfiguring.'
Invoke-PerastageBuild `
    -ConfigurePreset $presets.Configure `
    -BuildPreset $presets.Build `
    -ShouldBuild (-not $SkipBuild)
Write-Host 'Perastage Windows setup completed successfully.'
