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
    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    $process.WaitForExit()

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

Export-ModuleMember -Function Invoke-PerastageNativeCommandCapture, ConvertTo-PerastageNormalizedPathText, Test-PerastageRejectedWindowsBashPath, Test-PerastageBashProbe, Get-PerastageGitBashCandidatesFromGit, Resolve-PerastageGitBash
