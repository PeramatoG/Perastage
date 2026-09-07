param(
    [Parameter(Mandatory = $true)]
    [string]$ModulePath
)

$ErrorActionPreference = 'Stop'
Import-Module $ModulePath -Force

$root = Join-Path ([System.IO.Path]::GetTempPath()) "perastage-wx-secretstore-$([Guid]::NewGuid())"

# Writes a setup header in a synthetic vcpkg installed-tree fixture.
function Write-SetupHeader {
    param(
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Parameter(Mandatory = $true)][string]$Content
    )

    $path = Join-Path $root $RelativePath
    New-Item -ItemType Directory -Path (Split-Path -Parent $path) -Force | Out-Null
    Set-Content -LiteralPath $path -Value $Content
}

try {
    Write-SetupHeader -RelativePath 'include\wx\setup.h' -Content '#define wxUSE_SECRETSTORE 0'
    Write-SetupHeader -RelativePath 'debug\lib\mswud\wx\setup.h' -Content '#define wxUSE_SECRETSTORE 1'
    Write-SetupHeader -RelativePath 'lib\mswu\wx\setup.h' -Content '#define wxUSE_SECRETSTORE 1'
    Write-SetupHeader -RelativePath 'lib\mswud\wx\setup.h' -Content '#define wxUSE_SECRETSTORE 1'

    Assert-PerastageWxSecretStoreHeaders -InstalledTriplet $root

    Write-SetupHeader -RelativePath 'debug\lib\mswud\wx\setup.h' -Content '#define wxUSE_SECRETSTORE 0'
    try {
        Assert-PerastageWxSecretStoreHeaders -InstalledTriplet $root
        throw 'A generated Debug setup header with wxUSE_SECRETSTORE=0 unexpectedly passed.'
    } catch {
        if ($_.Exception.Message -notmatch 'Debug.*wxUSE_SECRETSTORE=0') {
            throw
        }
    }

    Write-SetupHeader -RelativePath 'debug\lib\mswud\wx\setup.h' -Content '#define wxUSE_WEBVIEW 1'
    try {
        Assert-PerastageWxSecretStoreHeaders -InstalledTriplet $root
        throw 'A generated Debug setup header without wxUSE_SECRETSTORE unexpectedly passed.'
    } catch {
        if ($_.Exception.Message -notmatch 'Debug.*wxUSE_SECRETSTORE is not defined') {
            throw
        }
    }
} finally {
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host 'OK: Windows wxWidgets generated setup-header discovery validates Debug and Release layouts.'
