#!/usr/bin/env python3
import argparse
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--powershell', required=True)
args = parser.parse_args()

with tempfile.TemporaryDirectory() as tmp:
    tmp_path = Path(tmp)
    child = tmp_path / 'native_child.py'
    child.write_text(
        "import sys\n"
        "sys.stdout.write('stdout-marker\\n')\n"
        "sys.stderr.write('stderr-marker\\n')\n"
        "sys.exit(int(sys.argv[1]))\n"
    )
    script = tmp_path / 'capture_test.ps1'
    script.write_text(f"""
$ErrorActionPreference = 'Stop'
Import-Module '{(ROOT / 'scripts/windows/PerastageWindowsBootstrap.psm1').as_posix()}' -Force
$ok = Invoke-PerastageNativeCommandCapture -FilePath '{Path(__import__('sys').executable).as_posix()}' -ArgumentList @('{child.as_posix()}', '0')
if ($ok.ExitCode -ne 0) {{ throw 'zero exit code was not captured' }}
if ($ok.StdOut -notmatch 'stdout-marker') {{ throw 'stdout was not captured' }}
if ($ok.StdErr -notmatch 'stderr-marker') {{ throw 'stderr was not captured' }}
if ($ok.Combined -notmatch 'stdout-marker' -or $ok.Combined -notmatch 'stderr-marker') {{ throw 'combined output was not captured' }}
$bad = Invoke-PerastageNativeCommandCapture -FilePath '{Path(__import__('sys').executable).as_posix()}' -ArgumentList @('{child.as_posix()}', '7')
if ($bad.ExitCode -ne 7) {{ throw 'non-zero exit code was not captured' }}
if ($bad.StdErr -notmatch 'stderr-marker') {{ throw 'non-zero stderr was not captured' }}
Write-Host 'OK: PowerShell native command capture preserves stdout, stderr, combined text, and exit codes.'
""")
    command = [args.powershell]
    if Path(args.powershell).name.lower().startswith('pwsh'):
        command += ['-NoLogo']
    command += ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(script)]
    result = subprocess.run(command, text=True, capture_output=True)
    if result.returncode != 0:
        raise SystemExit(result.stdout + result.stderr)
    print(result.stdout.strip())
