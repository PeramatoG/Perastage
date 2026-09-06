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

def require(pattern, *paths, flags=0):
    for path in paths:
        if re.search(pattern, read(path), flags):
            return
    raise SystemExit(f'Missing required pattern {pattern!r} in: {", ".join(paths)}')

def forbid(pattern, *paths, flags=0):
    for path in paths:
        text = read(path)
        match = re.search(pattern, text, flags)
        if match:
            raise SystemExit(f'Forbidden pattern {pattern!r} found in {path}: {match.group(0)!r}')

manifest = json.loads(read('vcpkg.json'))
baseline = manifest.get('builtin-baseline')
assert baseline == '0878b5224d4a4968940ee296a2e7fae2d3b62983', 'vcpkg.json must pin the expected builtin-baseline'
deps = manifest['dependencies']
wx = next((dep for dep in deps if isinstance(dep, dict) and dep.get('name') == 'wxwidgets'), None)
assert wx and 'secretstore' in wx.get('features', []), 'vcpkg.json must request wxwidgets[secretstore]'
gettext = next((dep for dep in deps if isinstance(dep, dict) and dep.get('name') == 'gettext'), None)
assert gettext, 'vcpkg.json must declare gettext'
assert gettext.get('host') is True, 'gettext must be a host dependency'
assert gettext.get('platform') == 'windows', 'gettext host tools must be Windows-only in the manifest'
assert 'tools' in gettext.get('features', []), 'gettext must request the tools feature'

windows_implementation = 'scripts/windows/PerastageWindowsBootstrap.ps1'
windows_roots = [windows_implementation, '.github/workflows/windows-installer.yml', 'docs/developer/build.md', 'docs/user/installation-windows.md', 'docs/user/troubleshooting.md', 'docs/developer/localization.md']
forbid(r'vcpkg(?:\.exe)?\s+install\s+"?gettext\[tools\]', *windows_roots, flags=re.I)

require(r'PERASTAGE_REQUIRE_SECURE_CREDENTIAL_STORE.*ON|PERASTAGE_REQUIRE_SECURE_CREDENTIAL_STORE=ON', '.github/workflows/windows-installer.yml', '.github/workflows/linux-installer.yml', '.github/workflows/arch-package.yml', '.github/workflows/macos-installer.yml', '.github/workflows/macos-15-manual-installer.yml', windows_implementation, 'packaging/arch/PKGBUILD')
require(r'bootstrap-vcpkg\.bat', '.github/workflows/windows-installer.yml')
require(r'checkout --force|checkout --detach', '.github/workflows/windows-installer.yml')
require(r'--x-install-root', '.github/workflows/windows-installer.yml')
require(r'VCPKG_INSTALLED_DIR', '.github/workflows/windows-installer.yml')
require(r'Initialize-X64MsvcEnvironment', windows_implementation)
require(r'VSCMD_ARG_HOST_ARCH.*x64|hostArch.*x64', windows_implementation)
require(r'VSCMD_ARG_TGT_ARCH.*x64|targetArch.*x64', windows_implementation)
require(r'for\\s\+x64|for\\s\*x64|for\\s\+x64', windows_implementation)
require(r'hostx64.*x64', windows_implementation, flags=re.I)
require(r'hostx86.*x86', windows_implementation, flags=re.I)
require(r'cached compiler Visual Studio root', windows_implementation)
require(r'VCPKG_MANIFEST_MODE=OFF|VCPKG_MANIFEST_MODE.*OFF', windows_implementation, 'CMakePresets.json', '.github/workflows/windows-installer.yml', '.github/workflows/linux-installer.yml', '.github/workflows/arch-package.yml', '.github/workflows/macos-installer.yml', '.github/workflows/macos-15-manual-installer.yml')
require(r'securestore-v2', '.github/workflows/windows-installer.yml', '.github/workflows/linux-installer.yml', '.github/workflows/arch-package.yml', '.github/workflows/macos-installer.yml', '.github/workflows/macos-15-manual-installer.yml')
require(r'0878b5224d4a4968940ee296a2e7fae2d3b62983', 'vcpkg.json')
require(r'get_vcpkg_baseline\.py vcpkg\.json', '.github/workflows/windows-installer.yml', '.github/workflows/linux-installer.yml', '.github/workflows/arch-package.yml', '.github/workflows/macos-installer.yml', '.github/workflows/macos-15-manual-installer.yml')
ci_workflow = read('.github/workflows/ci-tests.yml')
for platform, build_dir in (
    ('Linux', 'build/linux-debug'),
    ('Windows', 'build-windows-debug'),
    ('macOS', 'build/macos-debug'),
):
    commands = [
        line.strip()
        for line in ci_workflow.splitlines()
        if re.search(rf'\bctest\s+--test-dir\s+{re.escape(build_dir)}\b', line)
        and '--output-on-failure' in line
    ]
    assert len(commands) == 1, f'{platform} Debug CI must have one canonical complete-suite CTest command'
    assert not re.search(r'(?:^|\s)(?:-L|--label-regex|-R|--tests-regex|-E|--exclude-regex)(?:\s|=)', commands[0]), (
        f'{platform} Debug CI canonical CTest command must remain unfiltered: {commands[0]}'
    )
require(r'cmake\s+--build\s+build/macos-debug\s+--target\s+all\s+--verbose', '.github/workflows/ci-tests.yml')
require(r'add_executable\(gdtf_share_security_test\b', 'tests/CMakeLists.txt')
require(r'add_test\(NAME\s+GdtfShareSecurity\s+COMMAND\s+gdtf_share_security_test\)', 'tests/CMakeLists.txt')
require(r'add_executable\(credential_store_native_roundtrip_test\b', 'tests/CMakeLists.txt')
require(r'add_test\(NAME\s+CredentialStoreNativeRoundTrip\s+COMMAND\s+credential_store_native_roundtrip_test\)', 'tests/CMakeLists.txt')
require(r'libsecret-1-dev', '.github/workflows/linux-installer.yml')
require(r"'libsecret'", 'packaging/arch/PKGBUILD')
require(r'--manifest-root', '.github/workflows/macos-installer.yml', '.github/workflows/macos-15-manual-installer.yml')

presets = json.loads(read('CMakePresets.json'))
windows_presets = [preset for preset in presets['configurePresets'] if preset['name'].startswith('win-')]
assert {preset['name'] for preset in windows_presets} == {'win-x64-debug-ninja', 'win-x64-release-ninja'}
for preset in presets['configurePresets']:
    if preset['name'].startswith('win-x64-'):
        cache = preset.get('cacheVariables', {})
        assert preset.get('toolchainFile') == '${sourceDir}/cmake/PerastageWindowsVcpkgToolchain.cmake', preset['name']
        assert 'CMAKE_TOOLCHAIN_FILE' not in cache, preset['name']
        assert 'VCPKG_INSTALLED_DIR' not in cache, preset['name']
        assert cache.get('VCPKG_MANIFEST_MODE') == 'OFF', preset['name']
        assert cache.get('VCPKG_MANIFEST_INSTALL') == 'OFF', preset['name']
    if preset['name'] in {'win-x64-release-ninja', 'win-x64-debug-ninja'}:
        arch = preset.get('architecture', {})
        assert arch.get('value') == 'x64', preset['name']
        assert arch.get('strategy') == 'external', preset['name']
PY

echo 'OK: secure-store build, manifest, workflow, and Debug CI and Release builder policies are enforced.'
