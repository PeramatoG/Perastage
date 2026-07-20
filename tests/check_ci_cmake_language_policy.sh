#!/usr/bin/env bash
set -euo pipefail

python3 - <<'PY'
from pathlib import Path
import re

root = Path('CMakeLists.txt').read_text()
project_match = re.search(r'project\s*\((.*?)\)', root, re.S)
if not project_match:
    raise SystemExit('Root CMakeLists.txt does not declare project().')
project_body = re.sub(r'\s+', ' ', project_match.group(1))
if 'LANGUAGES C CXX' not in project_body:
    raise SystemExit('Root project() must enable both C and CXX.')
if 'CMP0177' not in root or 'cmake_policy(SET CMP0177 NEW)' not in root:
    raise SystemExit('Root CMakeLists.txt must set CMP0177 to NEW behind a policy guard.')

tests_cmake = Path('tests/CMakeLists.txt').read_text()
if re.search(r'(?m)^\s*(project|enable_language)\s*\(', tests_cmake):
    raise SystemExit('tests/CMakeLists.txt must not unconditionally call project() or enable_language().')

all_cmake = [p for p in Path('.').rglob('CMakeLists.txt') if '.git' not in p.parts and p != Path('CMakeLists.txt')]
for path in all_cmake:
    text = path.read_text(errors='ignore')
    if re.search(r'(?m)^\s*enable_language\s*\(', text):
        raise SystemExit(f'Nested enable_language() is not allowed: {path}')

ci = Path('.github/workflows/ci-tests.yml').read_text()
required_windows = [
    'VsDevCmd.bat', '-host_arch=x64 -arch=x64', 'CMAKE_C_COMPILER=cl.exe',
    'CMAKE_CXX_COMPILER=cl.exe', 'VCPKG_TARGET_TRIPLET=x64-windows',
    'CMakeConfigureLog.yaml', 'cmake-configure-windows-debug.log',
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
