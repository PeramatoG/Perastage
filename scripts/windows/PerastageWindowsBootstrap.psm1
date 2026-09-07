# Shared helpers for the local Windows x64 bootstrap workflow.

function Join-PerastageNativeArguments {
    param([string[]]$ArgumentList = @())

    $quoted = @()
    foreach ($argument in $ArgumentList) {
        if ($null -eq $argument) {
            $quoted += '""'
            continue
        }
        $escaped = $argument.Replace('"', '\"')
        $quoted += '"' + $escaped + '"'
    }
    return ($quoted -join ' ')
}

function Invoke-PerastageNativeCommandCapture {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [string[]]$ArgumentList = @(),
        [string]$WorkingDirectory = (Get-Location).Path
    )

    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $FilePath
    $startInfo.Arguments = Join-PerastageNativeArguments -ArgumentList $ArgumentList
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    [void]$process.Start()
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    $stdout = $stdoutTask.Result
    $stderr = $stderrTask.Result

    return [pscustomobject]@{
        StdOut = $stdout
        StdErr = $stderr
        Combined = $stdout + $stderr
        ExitCode = $process.ExitCode
    }
}

function ConvertTo-PerastageNormalizedPathText {
    param([string]$PathText)

    if ([string]::IsNullOrWhiteSpace($PathText)) {
        return ''
    }
    return $PathText.Trim('"').Replace('/', '\').TrimEnd('\').ToLowerInvariant()
}

function Test-PerastageRejectedWindowsBashPath {
    param([string]$PathText)

    $normalized = ConvertTo-PerastageNormalizedPathText $PathText
    return ($normalized -match '\windows\system32\bash\.exe$' -or $normalized -match '\windowsapps\bash\.exe$')
}

function Test-PerastageBashProbe {
    param([Parameter(Mandatory = $true)][string]$BashPath)

    if (-not (Test-Path -LiteralPath $BashPath)) {
        return $false
    }
    $capture = Invoke-PerastageNativeCommandCapture -FilePath $BashPath -ArgumentList @('--noprofile', '--norc', '-c', "printf 'perastage-git-bash-ok`n'")
    return ($capture.ExitCode -eq 0 -and $capture.StdOut.Trim() -eq 'perastage-git-bash-ok')
}

function Get-PerastageGitBashCandidatesFromGit {
    param([Parameter(Mandatory = $true)][string]$GitPath)

    $gitBin = Split-Path -Parent $GitPath
    $gitRoot = Split-Path -Parent $gitBin
    $candidates = @(
        (Join-Path $gitRoot 'bin\bash.exe'),
        (Join-Path $gitRoot 'usr\bin\bash.exe')
    )
    $gitParent = Split-Path -Parent $gitRoot
    if ($gitParent) {
        $candidates += @(
            (Join-Path $gitParent 'bin\bash.exe'),
            (Join-Path $gitParent 'usr\bin\bash.exe')
        )
    }
    return $candidates | Select-Object -Unique
}

function Resolve-PerastageGitBash {
    param([string]$ExplicitBash = '')

    if (-not [string]::IsNullOrWhiteSpace($ExplicitBash)) {
        if (Test-PerastageRejectedWindowsBashPath $ExplicitBash) {
            throw "BASH_EXECUTABLE must be Git Bash on Windows, not a WSL or application-alias launcher: $ExplicitBash"
        }
        if (-not (Test-PerastageBashProbe -BashPath $ExplicitBash)) {
            throw "Explicit Git Bash candidate failed the non-login shell probe: $ExplicitBash"
        }
        return (Resolve-Path -LiteralPath $ExplicitBash).Path
    }

    $gitCommand = Get-Command git.exe -ErrorAction SilentlyContinue
    if (-not $gitCommand) {
        $gitCommand = Get-Command git -ErrorAction SilentlyContinue
    }
    if ($gitCommand) {
        foreach ($candidate in (Get-PerastageGitBashCandidatesFromGit -GitPath $gitCommand.Source)) {
            if ((-not (Test-PerastageRejectedWindowsBashPath $candidate)) -and (Test-PerastageBashProbe -BashPath $candidate)) {
                return (Resolve-Path -LiteralPath $candidate).Path
            }
        }
    }

    throw 'Git Bash could not be resolved. Install Git for Windows or pass -DBASH_EXECUTABLE=<Git for Windows bash.exe>; WSL and WindowsApps bash launchers are not supported.'
}

# Returns generated wxWidgets MSW setup headers grouped by build configuration.
function Get-PerastageWxSetupHeaderGroups {
    param([Parameter(Mandatory = $true)][string]$InstalledTriplet)

    $legacyHeader = Join-Path $InstalledTriplet 'include\wx\msw\setup.h'
    $sharedHeaders = @()
    if (Test-Path -LiteralPath $legacyHeader -PathType Leaf) {
        $sharedHeaders += (Resolve-Path -LiteralPath $legacyHeader).Path
    }

    $groups = [ordered]@{}
    foreach ($configuration in @('Debug', 'Release')) {
        $libraryRoot = if ($configuration -eq 'Debug') {
            Join-Path $InstalledTriplet 'debug\lib'
        } else {
            Join-Path $InstalledTriplet 'lib'
        }
        $headers = @($sharedHeaders)
        if (Test-Path -LiteralPath $libraryRoot -PathType Container) {
            $headers += Get-ChildItem -LiteralPath $libraryRoot -Directory -Filter 'msw*' |
                ForEach-Object { Join-Path $_.FullName 'wx\setup.h' } |
                Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
                ForEach-Object { (Resolve-Path -LiteralPath $_).Path }
        }
        $groups[$configuration] = @($headers | Select-Object -Unique)
    }
    return $groups
}

# Validates secret-store support in every generated wxWidgets MSW configuration header.
function Assert-PerastageWxSecretStoreHeaders {
    param([Parameter(Mandatory = $true)][string]$InstalledTriplet)

    $groups = Get-PerastageWxSetupHeaderGroups -InstalledTriplet $InstalledTriplet
    $failures = @()
    foreach ($configuration in @('Debug', 'Release')) {
        $headers = @($groups[$configuration])
        if ($headers.Count -eq 0) {
            $expectedRoot = if ($configuration -eq 'Debug') { 'debug\lib\msw*\wx\setup.h' } else { 'lib\msw*\wx\setup.h' }
            $failures += "$configuration`: no generated setup header was found under '$(Join-Path $InstalledTriplet $expectedRoot)' or the legacy 'include\wx\msw\setup.h'."
            continue
        }
        foreach ($header in $headers) {
            $content = Get-Content -LiteralPath $header -Raw
            if ($content -match '#\s*define\s+wxUSE_SECRETSTORE\s+1(?:\s|$)') {
                Write-Host "wxWidgets $configuration setup header: $header (wxUSE_SECRETSTORE=1)"
                continue
            }
            $observed = if ($content -match '#\s*define\s+wxUSE_SECRETSTORE\s+([^\s/]+)') {
                "wxUSE_SECRETSTORE=$($Matches[1])"
            } else {
                'wxUSE_SECRETSTORE is not defined'
            }
            $failures += "$configuration`: '$header' ($observed)."
        }
    }

    if ($failures.Count -gt 0) {
        $details = $failures -join [Environment]::NewLine
        throw "wxWidgets generated MSW setup-header validation failed. Inspected configuration-specific headers; the generic 'include\wx\setup.h' is not a generated platform configuration and was ignored.$([Environment]::NewLine)$details$([Environment]::NewLine)Rebuild wxWidgets with the secretstore feature in the selected vcpkg installation."
    }
}

Export-ModuleMember -Function Invoke-PerastageNativeCommandCapture, ConvertTo-PerastageNormalizedPathText, Test-PerastageRejectedWindowsBashPath, Test-PerastageBashProbe, Get-PerastageGitBashCandidatesFromGit, Resolve-PerastageGitBash, Get-PerastageWxSetupHeaderGroups, Assert-PerastageWxSecretStoreHeaders
