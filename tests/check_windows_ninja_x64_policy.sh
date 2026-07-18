#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

python3 - <<'PY'
import json
from pathlib import Path
presets = json.loads(Path('CMakePresets.json').read_text())
by_name = {preset['name']: preset for preset in presets['configurePresets']}
for name in ('win-x64-debug-ninja', 'win-x64-release-ninja'):
    preset = by_name[name]
    assert preset.get('generator') == 'Ninja', name
    assert preset.get('architecture', {}).get('value') == 'x64', name
    assert preset.get('architecture', {}).get('strategy') == 'external', name
    assert preset.get('cacheVariables', {}).get('VCPKG_TARGET_TRIPLET') == 'x64-windows', name
for name in ('win-x64-debug', 'win-x64-release'):
    preset = by_name[name]
    assert preset.get('architecture', {}).get('value') == 'x64', name
    assert preset.get('architecture', {}).get('strategy') == 'set', name
PY

python3 - <<'PY'
from pathlib import Path
script = Path('setup_windows.ps1').read_text()
required = [
    'VSCMD_ARG_HOST_ARCH',
    'VSCMD_ARG_TGT_ARCH',
    'cl.exe banner does not identify an x64 target',
    'hostx64\\\\x64',
    'hostx86\\\\x86',
    'cached compiler Visual Studio root',
    'Remove-Item -LiteralPath $BuildDirectory -Recurse -Force',
    'Test-PerastageLocalPresetsCompatible',
    'The file was not modified',
]
for needle in required:
    assert needle in script, needle
assert "Write-Host 'MSVC compiler architecture: x64'" not in script, 'setup must not print unconditional x64 status'
assert 'local-win-x64-debug-ninja' in script and 'local-win-x64-release-ninja' in script
PY

echo 'OK: Windows Ninja presets and setup script enforce x64 compiler/cache policy.'
