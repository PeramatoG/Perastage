#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
repo_root="${PERASTAGE_POLICY_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$repo_root"

run_test_python - <<'PY'
import json
import os
import re
import time
from pathlib import Path

ROOT = Path('.')

def read(path):
    return (ROOT / path).read_text(errors='ignore')

presets = json.loads(read('CMakePresets.json'))
windows_configure = [preset for preset in presets['configurePresets'] if preset['name'].startswith('win-')]
assert {preset['name'] for preset in windows_configure} == {'win-x64-debug-ninja', 'win-x64-release-ninja'}, windows_configure
for preset in windows_configure:
    name = preset['name']
    cache = preset.get('cacheVariables', {})
    assert preset.get('generator') == 'Ninja', name
    assert preset.get('architecture', {}).get('value') == 'x64', name
    assert preset.get('architecture', {}).get('strategy') == 'external', name
    assert preset.get('toolchainFile') == '${sourceDir}/cmake/PerastageWindowsVcpkgToolchain.cmake', name
    assert 'CMAKE_TOOLCHAIN_FILE' not in cache, name
    assert cache.get('VCPKG_TARGET_TRIPLET') == 'x64-windows', name
    assert cache.get('VCPKG_MANIFEST_MODE') == 'OFF', name
    assert cache.get('VCPKG_MANIFEST_INSTALL') == 'OFF', name
    assert 'VCPKG_INSTALLED_DIR' not in cache, name
windows_build = [preset for preset in presets['buildPresets'] if preset['name'].startswith('win-')]
assert {preset['configurePreset'] for preset in windows_build} <= {'win-x64-debug-ninja', 'win-x64-release-ninja'}, windows_build

launcher = read('setup_windows.ps1')
script = read('scripts/windows/PerastageWindowsBootstrap.ps1')
assert len(launcher.splitlines()) <= 40, 'root Windows setup launcher must remain small'
assert "scripts\\windows\\PerastageWindowsBootstrap.ps1" in launcher
assert '& $ImplementationScript @PSBoundParameters' in launcher
required = [
    "[string]$VcpkgRoot = ''",
    '$Root = $env:VCPKG_ROOT',
    '$env:VCPKG_ROOT = $resolvedVcpkg.Root',
    'could not resolve an external classic vcpkg checkout',
    'Get-UserWideVcpkgRoot',
    'vcpkg.path.txt',
    'Test-VisualStudioVcpkgRoot',
    'VSCMD_ARG_HOST_ARCH',
    'VSCMD_ARG_TGT_ARCH',
    'cl.exe banner does not identify an x64 target',
    r'hostx64\\x64',
    r'hostx86\\x86',
    'cached compiler Visual Studio root',
    'VCPKG_MANIFEST_MODE=OFF',
    'VCPKG_MANIFEST_INSTALL=OFF',
    'Remove-Item -LiteralPath $BuildDirectory -Recurse -Force',
    'Resolve-ClassicVcpkgInstallation',
    'Test-PerastageVcpkgDependencies',
    'Import-Module $BootstrapModulePath -Force',
    'Resolve-PerastageGitBash',
    '-DBASH_EXECUTABLE=',
    'Invoke-PerastageNativeCommandCapture',
    '[string]$BashExecutable = $env:BASH_EXECUTABLE',
    'Resolve-PerastageGitBash -ExplicitBash $BashExecutable',
    'PerastageWindowsBootstrap.psm1',
]
for needle in required:
    assert needle in script, needle
for forbidden in [
    'Write-PerastageCMakeUserPresets',
    'Test-PerastageLocalPresetsCompatible',
    'git clone',
    'bootstrap-vcpkg.bat',
    'checkout --detach',
    '--x-install-root',
    'CMakeUserPresets.json',
    '.tools\\vcpkg',
    'vcpkg_installed',
]:
    assert forbidden not in script, forbidden
assert "Write-Host 'MSVC compiler architecture: x64'" not in script, 'setup must not print unconditional x64 status'

assert '(& cl.exe 2>&1 | Out-String)' not in script, 'cl.exe banner capture must not depend on PowerShell error stream conversion'
assert '-DBASH_EXECUTABLE="$GitBashPath"' in script, 'setup must pass resolved Git Bash into CMake configure'
assert "Resolve-PerastageGitBash" in script, 'setup must resolve Git Bash explicitly'
bootstrap = read('scripts/windows/PerastageWindowsBootstrap.psm1')
for needle in [
    'System.Diagnostics.ProcessStartInfo',
    'Join-PerastageNativeArguments',
    'RedirectStandardOutput = $true',
    'RedirectStandardError = $true',
    '$startInfo.Arguments = Join-PerastageNativeArguments',
    'ReadToEndAsync()',
    'Get-PerastageGitBashCandidatesFromGit',
    'Test-PerastageRejectedWindowsBashPath',
    'WindowsApps bash launchers are not supported',
]:
    assert needle in bootstrap, needle

excluded_roots = {'.git', '.vcpkg-cache', '.vcpkg-root', '.tools', 'build', 'out', 'third_party', 'vcpkg', 'vcpkg_installed'}
excluded_prefixes = ('build-', 'cmake-build-')

def is_excluded_dirname(name):
    return name in excluded_roots or any(name.startswith(prefix) for prefix in excluded_prefixes)

def iter_first_party_files(root):
    for current_root, dirnames, filenames in os.walk(root):
        dirnames[:] = [name for name in dirnames if not is_excluded_dirname(name)]
        current = Path(current_root)
        for filename in filenames:
            path = current / filename
            if path == ROOT / 'tests/check_windows_ninja_x64_policy.sh':
                continue
            yield path

scan_start = time.monotonic()
scanned_files = 0
for path in iter_first_party_files(ROOT):
    if not path.is_file():
        continue
    scanned_files += 1
    try:
        text = path.read_text(errors='ignore')
    except OSError:
        continue
    if 'local-win-' in text:
        raise SystemExit(f'Tracked files must not reference generated local Windows presets: {path}')
scan_elapsed = time.monotonic() - scan_start
print(f'Checked {scanned_files} first-party files for generated Windows preset references in {scan_elapsed:.3f}s.')

for pattern in [r'&\s+\$[^\n]*vcpkg[^\n]*\s+install\b', r"vcpkg(?:\.exe)?[\"\']?\s+install\b"]:
    match = re.search(pattern, script, re.IGNORECASE)
    assert not match, match.group(0)
PY

echo 'OK: Windows Ninja presets and setup script enforce classic x64 vcpkg validation policy.'
