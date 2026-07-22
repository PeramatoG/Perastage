#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

run_test_python - <<'PY'
import json
import re
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
    assert cache.get('CMAKE_TOOLCHAIN_FILE') == 'C:/vcpkg/scripts/buildsystems/vcpkg.cmake', name
    assert cache.get('VCPKG_TARGET_TRIPLET') == 'x64-windows', name
    assert cache.get('VCPKG_MANIFEST_MODE') == 'OFF', name
    assert cache.get('VCPKG_MANIFEST_INSTALL') == 'OFF', name
    assert 'VCPKG_INSTALLED_DIR' not in cache, name
windows_build = [preset for preset in presets['buildPresets'] if preset['name'].startswith('win-')]
assert {preset['configurePreset'] for preset in windows_build} <= {'win-x64-debug-ninja', 'win-x64-release-ninja'}, windows_build

script = read('setup_windows.ps1')
required = [
    "[string]$VcpkgRoot = 'C:\\vcpkg'",
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

ignored_roots = {'.git', 'build', 'out', 'vcpkg', '.vcpkg-cache'}
for path in ROOT.rglob('*'):
    if not path.is_file() or ignored_roots.intersection(path.parts) or path == ROOT / 'tests/check_windows_ninja_x64_policy.sh':
        continue
    try:
        text = path.read_text(errors='ignore')
    except OSError:
        continue
    if 'local-win-' in text:
        raise SystemExit(f'Tracked files must not reference generated local Windows presets: {path}')

for pattern in [r'&\s+\$[^\n]*vcpkg[^\n]*\s+install\b', r"vcpkg(?:\.exe)?[\"\']?\s+install\b"]:
    match = re.search(pattern, script, re.IGNORECASE)
    assert not match, match.group(0)
PY

echo 'OK: Windows Ninja presets and setup script enforce classic x64 vcpkg validation policy.'
