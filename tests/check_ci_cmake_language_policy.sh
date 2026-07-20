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

windows = Path('.github/workflows/windows-installer.yml').read_text()
required_windows = [
    'VsDevCmd.bat', '-host_arch=x64 -arch=x64', 'CMAKE_C_COMPILER=cl.exe',
    'CMAKE_CXX_COMPILER=cl.exe', 'CMAKE_C_COMPILER_ID:INTERNAL=MSVC',
    'CMAKE_CXX_COMPILER_ID:INTERNAL=MSVC', 'c:[\\\\/](mingw64|msys64)',
    'CMakeConfigureLog.yaml', 'cmake-configure-windows-security.log',
]
for needle in required_windows:
    if needle not in windows:
        raise SystemExit(f'Windows secure-store workflow is missing policy marker: {needle}')

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
        raise SystemExit(f'{path} final diagnostic upload must be after build/package/test operations.')

release_gate_configs = {
    '.github/workflows/linux-installer.yml': 'cmake-configure-linux.log',
    '.github/workflows/macos-installer.yml': 'cmake-configure-macos26.log',
    '.github/workflows/macos-15-manual-installer.yml': 'cmake-configure-macos15.log',
    '.github/workflows/arch-package.yml': 'cmake-configure-arch-release-gate.log',
    '.github/workflows/windows-installer.yml': 'cmake-configure-windows-security.log',
}
for path, log_name in release_gate_configs.items():
    text = Path(path).read_text()
    if log_name not in text or '-DBUILD_TESTING=ON' not in text:
        raise SystemExit(f'{path} must keep BUILD_TESTING=ON and configure logging for release-gate paths.')

print('OK: CI CMake language, compiler, diagnostics, and release-gate policies are enforced.')
PY
