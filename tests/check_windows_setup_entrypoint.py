#!/usr/bin/env python3
"""Verify the stable Windows setup launcher and its delegation contract."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
LAUNCHER = ROOT / "setup_windows.ps1"
IMPLEMENTATION = ROOT / "scripts/windows/PerastageWindowsBootstrap.ps1"

launcher_text = LAUNCHER.read_text(encoding="utf-8")
implementation_text = IMPLEMENTATION.read_text(encoding="utf-8")

assert len(launcher_text.splitlines()) <= 40
assert "$PSScriptRoot" in launcher_text
assert "scripts\\windows\\PerastageWindowsBootstrap.ps1" in launcher_text
assert "Test-Path -LiteralPath $ImplementationScript -PathType Leaf" in launcher_text
assert "& $ImplementationScript @PSBoundParameters" in launcher_text
assert "Initialize-X64MsvcEnvironment" not in launcher_text
assert "Resolve-ClassicVcpkgInstallation" not in launcher_text

expected_parameters = {
    "Configuration": "[string]$Configuration = 'Debug'",
    "VcpkgRoot": "[string]$VcpkgRoot = ''",
    "VisualStudioPath": "[string]$VisualStudioPath = ''",
    "VisualStudioVersion": "[string]$VisualStudioVersion = ''",
    "BashExecutable": "[string]$BashExecutable = $env:BASH_EXECUTABLE",
    "SkipBuild": "[switch]$SkipBuild",
    "CleanBuild": "[switch]$CleanBuild",
}
for declaration in expected_parameters.values():
    assert declaration in launcher_text, declaration

for responsibility in (
    "Initialize-X64MsvcEnvironment",
    "Resolve-ClassicVcpkgInstallation",
    "Test-PerastageVcpkgDependencies",
    "Resolve-PerastageGitBash",
    "Get-CMakePresetNames",
    "Reset-IncompatibleCMakeCache",
    "Invoke-PerastageBuild",
):
    assert responsibility in implementation_text, responsibility

powershell = shutil.which("pwsh") or shutil.which("powershell")
if powershell:
    with tempfile.TemporaryDirectory() as temporary_directory:
        temporary = Path(temporary_directory)
        repository = temporary / "Repository With Spaces"
        scripts = repository / "scripts/windows"
        outside = temporary / "Outside Working Directory"
        scripts.mkdir(parents=True)
        outside.mkdir()
        shutil.copy2(LAUNCHER, repository / LAUNCHER.name)
        result_path = temporary / "forwarded values.json"
        stub = r'''param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [string]$VcpkgRoot = '',
    [string]$VisualStudioPath = '',
    [string]$VisualStudioVersion = '',
    [string]$BashExecutable = $env:BASH_EXECUTABLE,
    [switch]$SkipBuild,
    [switch]$CleanBuild
)
if ($VisualStudioVersion -eq 'fail') { throw 'delegated fixture failure' }
[ordered]@{
    Configuration = $Configuration
    VcpkgRoot = $VcpkgRoot
    VisualStudioPath = $VisualStudioPath
    VisualStudioVersion = $VisualStudioVersion
    BashExecutable = $BashExecutable
    SkipBuild = [bool]$SkipBuild
    CleanBuild = [bool]$CleanBuild
    WorkingDirectory = (Get-Location).Path
} | ConvertTo-Json | Set-Content -LiteralPath $env:PERASTAGE_ENTRYPOINT_RESULT
'''
        (scripts / IMPLEMENTATION.name).write_text(stub, encoding="utf-8")
        environment = {"PERASTAGE_ENTRYPOINT_RESULT": str(result_path)}

        command = [
            powershell,
            "-NoProfile",
            "-File",
            str(repository / LAUNCHER.name),
            "-Configuration",
            "Release",
            "-VcpkgRoot",
            str(temporary / "vcpkg root"),
            "-VisualStudioPath",
            str(temporary / "Visual Studio"),
            "-VisualStudioVersion",
            "[17.0,18.0)",
            "-BashExecutable",
            str(temporary / "Git Bash/bash.exe"),
            "-SkipBuild",
            "-CleanBuild",
        ]
        completed = subprocess.run(command, cwd=outside, env={**os.environ, **environment}, text=True, capture_output=True)
        assert completed.returncode == 0, completed.stdout + completed.stderr
        forwarded = json.loads(result_path.read_text(encoding="utf-8-sig"))
        assert forwarded["Configuration"] == "Release"
        assert forwarded["VcpkgRoot"] == str(temporary / "vcpkg root")
        assert forwarded["VisualStudioPath"] == str(temporary / "Visual Studio")
        assert forwarded["VisualStudioVersion"] == "[17.0,18.0)"
        assert forwarded["BashExecutable"] == str(temporary / "Git Bash/bash.exe")
        assert forwarded["SkipBuild"] is True and forwarded["CleanBuild"] is True
        assert Path(forwarded["WorkingDirectory"]) == outside

        default_run = subprocess.run(
            [powershell, "-NoProfile", "-File", str(repository / LAUNCHER.name)],
            cwd=outside,
            env={**os.environ, **environment, "BASH_EXECUTABLE": "default bash.exe"},
            text=True,
            capture_output=True,
        )
        assert default_run.returncode == 0, default_run.stdout + default_run.stderr
        defaults = json.loads(result_path.read_text(encoding="utf-8-sig"))
        assert defaults["Configuration"] == "Debug"
        assert defaults["BashExecutable"] == "default bash.exe"
        assert defaults["SkipBuild"] is False and defaults["CleanBuild"] is False

        delegated_failure = subprocess.run(
            [powershell, "-NoProfile", "-File", str(repository / LAUNCHER.name), "-VisualStudioVersion", "fail"],
            cwd=outside,
            text=True,
            capture_output=True,
        )
        assert delegated_failure.returncode != 0
        assert "delegated fixture failure" in delegated_failure.stderr

        (scripts / IMPLEMENTATION.name).unlink()
        missing = subprocess.run(
            [powershell, "-NoProfile", "-File", str(repository / LAUNCHER.name)],
            cwd=outside,
            text=True,
            capture_output=True,
        )
        assert missing.returncode != 0
        assert "Windows setup implementation was not found" in missing.stderr

print("OK: Windows setup root launcher preserves its public contract and delegation boundary.")
