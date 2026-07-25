#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
repo_root="${PERASTAGE_POLICY_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$repo_root"

run_test_python - <<'PY'
from pathlib import Path
import os
import re
import time

root_path = Path('CMakeLists.txt')
root = root_path.read_text()
project_match = re.search(r'project\s*\((.*?)\)', root, re.S)
if not project_match:
    raise SystemExit('Root CMakeLists.txt does not declare project().')
project_body = re.sub(r'\s+', ' ', project_match.group(1))
if 'LANGUAGES C CXX' not in project_body:
    raise SystemExit('Root project() must enable both C and CXX.')
if 'CMP0177' not in root or 'cmake_policy(SET CMP0177 NEW)' not in root:
    raise SystemExit('Root CMakeLists.txt must set CMP0177 to NEW behind a policy guard.')

excluded_roots = {'.git', '.vcpkg-cache', 'vcpkg', 'build', 'out', 'third_party'}
excluded_prefixes = ('build-', 'cmake-build-')

def is_excluded_dirname(name: str) -> bool:
    return name in excluded_roots or any(name.startswith(prefix) for prefix in excluded_prefixes)

def iter_first_party_cmake_lists(root: Path):
    for current_root, dirnames, filenames in os.walk(root):
        dirnames[:] = [name for name in dirnames if not is_excluded_dirname(name)]
        current = Path(current_root)
        if current == root:
            continue
        if 'CMakeLists.txt' in filenames:
            yield current / 'CMakeLists.txt'

tests_cmake = Path('tests/CMakeLists.txt')
if tests_cmake.exists():
    text = tests_cmake.read_text()
    if re.search(r'(?m)^\s*(project|enable_language)\s*\(', text):
        raise SystemExit('tests/CMakeLists.txt must not unconditionally call project() or enable_language().')

scan_start = time.monotonic()
all_cmake = list(iter_first_party_cmake_lists(Path('.')))
scan_elapsed = time.monotonic() - scan_start
print(f'Checked {len(all_cmake)} first-party CMakeLists.txt files in {scan_elapsed:.3f}s.')
for path in all_cmake:
    text = path.read_text(errors='ignore')
    if re.search(r'(?m)^\s*enable_language\s*\(', text):
        raise SystemExit(f'Nested enable_language() is not allowed: {path}')

ci = Path('.github/workflows/ci-tests.yml').read_text()
required_windows = [
    'VsDevCmd.bat', '-host_arch=x64 -arch=x64', 'Get-Command cl.exe -ErrorAction Stop',
    '--c-compiler "$clExecutable"', '--cxx-compiler "$clExecutable"', 'VCPKG_TARGET_TRIPLET=x64-windows',
    'CMakeConfigureLog.yaml', 'cmake-configure-windows-debug.log',
    'validate_cmake_toolchain.py',
]
for needle in required_windows:
    if needle not in ci:
        raise SystemExit(f'Windows Debug CI is missing policy marker: {needle}')

workflow_paths = [
    Path('.github/workflows/windows-installer.yml'), Path('.github/workflows/linux-installer.yml'),
    Path('.github/workflows/macos-installer.yml'), Path('.github/workflows/macos-15-manual-installer.yml'),
    Path('.github/workflows/arch-package.yml'),
]
for path in workflow_paths:
    text = path.read_text()
    if 'Upload CI diagnostics' in text or 'Upload macOS build diagnostics' in text:
        raise SystemExit(f'{path} still contains obsolete diagnostic upload steps.')
    if 'Upload final CI diagnostics' not in text:
        raise SystemExit(f'{path} is missing a final diagnostic upload step.')
    if 'CMakeConfigureLog.yaml' not in text or 'out/ci-logs/**' not in text:
        raise SystemExit(f'{path} does not upload modern CMake configure diagnostics.')
    final_upload = text.rfind('Upload final CI diagnostics')
    last_operation = max(text.rfind('cmake '), text.rfind('ctest '), text.rfind('makepkg'), text.rfind('appimagetool'), text.rfind('ISCC'))
    if final_upload < last_operation:
        raise SystemExit(f'{path} final diagnostic upload must be after build/package operations.')
    if '-DBUILD_TESTING=ON' in text or 'release-gate' in text:
        raise SystemExit(f'{path} must be a Release-only package builder without release-gate tests.')
    if 'PERASTAGE_ENABLE_COMPILER_CACHE=OFF' not in text:
        raise SystemExit(f'{path} must disable uncontrolled compiler-cache discovery.')
    if path.name != 'arch-package.yml' and '-DBUILD_TESTING=OFF' not in text:
        raise SystemExit(f'{path} must configure with BUILD_TESTING=OFF.')

if 'CMAKE_BUILD_TYPE=Debug' not in ci or '-DBUILD_TESTING=ON' not in ci or 'must not compile with NDEBUG' not in ci:
    raise SystemExit('ci-tests.yml must be the Debug test owner and must protect assert-based tests.')

print('OK: CI CMake language, compiler, diagnostics, and release-gate policies are enforced.')
PY
