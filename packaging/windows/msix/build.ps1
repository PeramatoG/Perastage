# Consume the validated Windows Release stage; never rebuild or sign Store output.
[CmdletBinding()]
param(
    [string]$StageDir,
    [string]$OutputDir,
    [string]$IdentityFile,
    [string]$SdkBinDir,
    [string]$VCLibsPackage,
    [string]$Python = 'python'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
if (-not $StageDir) { $StageDir = Join-Path $repoRoot 'out/install/Release' }
if (-not $OutputDir) { $OutputDir = Join-Path $repoRoot 'out/msix' }
$StageDir = (Resolve-Path $StageDir).Path
$OutputDir = [System.IO.Path]::GetFullPath($OutputDir)
if ($OutputDir.StartsWith($StageDir + [System.IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
    $OutputDir.Equals($StageDir, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Store output must be outside the validated release stage'
}
$helper = Join-Path $PSScriptRoot 'msix.py'
$nameOutput = & $Python $helper name
if ($LASTEXITCODE -ne 0) { throw 'MSIX version validation failed' }
$packageName = [string]$nameOutput
$package = Join-Path $OutputDir $packageName
if (Test-Path $package) { throw "Refusing to overwrite an existing Store artifact: $package" }

# Resolve SDK versions numerically, rather than relying on PATH or lexical order.
if (-not $SdkBinDir) {
    $sdkRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits/10/bin'
    $candidates = @(Get-ChildItem $sdkRoot -Directory | Where-Object {
        $_.Name -match '^10\.0\.[0-9]+\.0$' -and
        (Test-Path (Join-Path $_.FullName 'x64/makeappx.exe'))
    } | Sort-Object { [version]$_.Name } -Descending)
    if ($candidates.Count -eq 0) { throw 'Windows SDK x64 MakeAppx.exe is required' }
    $SdkBinDir = Join-Path $candidates[0].FullName 'x64'
}
$makeappx = Join-Path $SdkBinDir 'makeappx.exe'
if (-not (Test-Path $makeappx -PathType Leaf)) { throw "Missing SDK tool: $makeappx" }
Write-Host "Store packaging SDK: $makeappx"

if (-not $VCLibsPackage) {
    $frameworkRoot = Join-Path ${env:ProgramFiles(x86)} 'Microsoft SDKs/Windows Kits/10/ExtensionSDKs/Microsoft.VCLibs.Desktop/14.0/Appx/Retail'
    if (-not (Test-Path $frameworkRoot -PathType Container)) {
        throw 'Retail Desktop VCLibs framework is not installed; supply -VCLibsPackage matching the release toolchain'
    }
    $frameworks = @(Get-ChildItem $frameworkRoot -Recurse -File -Filter '*x64*.appx')
    if ($frameworks.Count -ne 1) {
        throw 'Supply -VCLibsPackage with the Microsoft retail x64 Desktop VCLibs framework matching the release toolchain'
    }
    $VCLibsPackage = $frameworks[0].FullName
}
if (-not (Test-Path $VCLibsPackage -PathType Leaf)) { throw "Missing Desktop VCLibs framework: $VCLibsPackage" }
Write-Host "Desktop VCLibs framework: $VCLibsPackage"

# The existing release workflow already smoke-tests this same stage. Repeat only
# these cheap preflight checks for maintainers invoking this script independently.
$version = (Get-Content (Join-Path $repoRoot 'VERSION') -Raw).Trim()
$cli = Join-Path $StageDir 'perastage-cli.exe'
if (-not (Test-Path $cli -PathType Leaf)) { throw 'Missing staged perastage-cli.exe' }
$cliVersion = & $cli --version
if ($LASTEXITCODE -ne 0 -or [string]$cliVersion -notmatch "(?<![0-9.])$([regex]::Escape($version))(?![0-9.])") {
    throw "Staged CLI version does not match canonical VERSION ($version)"
}
& $cli --help
if ($LASTEXITCODE -ne 0) { throw 'Staged CLI --help failed' }
& $Python (Join-Path $repoRoot 'packaging/windows/verify_executable_manifests.py') `
    --gui (Join-Path $StageDir 'Perastage.exe') --cli $cli --mt (Join-Path $SdkBinDir 'mt.exe')
if ($LASTEXITCODE -ne 0) { throw 'Staged Windows executable manifest validation failed' }
& $Python (Join-Path $repoRoot 'packaging/verify_localization.py') --asset-root $StageDir
if ($LASTEXITCODE -ne 0) { throw 'Release localization payload validation failed' }

$workRoot = Join-Path $repoRoot ('out/msix-work/' + [guid]::NewGuid().ToString('N'))
$layout = Join-Path $workRoot 'layout'
$prepareArgs = @($helper, 'prepare', '--stage', $StageDir, '--layout', $layout, '--vclibs-package', $VCLibsPackage)
if ($IdentityFile) { $prepareArgs += @('--identity', $IdentityFile) }
try {
    & $Python @prepareArgs
    if ($LASTEXITCODE -ne 0) { throw 'MSIX identity/payload/manifest/asset validation failed' }
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
    # Full schema/semantic validation stays enabled: never pass /nv.
    & $makeappx pack /d $layout /p $package /h SHA256 /no
    if ($LASTEXITCODE -ne 0) { throw 'MakeAppx packaging/schema validation failed' }
    & $Python $helper verify --layout $layout --package $package
    if ($LASTEXITCODE -ne 0) { throw 'MSIX output content validation failed' }
    Write-Host "Unsigned Store submission artifact: $package"
} catch {
    # Never leave a failed package available for artifact upload.
    if (Test-Path $package) { Remove-Item $package -Force }
    throw
} finally {
    if (Test-Path $workRoot) { Remove-Item $workRoot -Recurse -Force }
}
