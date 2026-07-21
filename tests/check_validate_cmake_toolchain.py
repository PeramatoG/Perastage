#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / '.github/scripts/validate_cmake_toolchain.py'


def write_layout(root: Path, version: str, compiler_root: str) -> None:
    (root / 'CMakeFiles' / version).mkdir(parents=True)
    (root / 'CMakeCache.txt').write_text('\n'.join([
        'CMAKE_GENERATOR:INTERNAL=Ninja',
        'CMAKE_BUILD_TYPE:STRING=Debug',
        f'CMAKE_C_COMPILER:FILEPATH={compiler_root}/cl.exe',
        f'CMAKE_CXX_COMPILER:FILEPATH={compiler_root}/cl.exe',
    ]))
    for lang, name in [('C', 'C'), ('CXX', 'CXX')]:
        (root / 'CMakeFiles' / version / f'CMake{name}Compiler.cmake').write_text('\n'.join([
            f'set(CMAKE_{lang}_COMPILER_ID MSVC)',
            f'set(CMAKE_{lang}_COMPILER_ARCHITECTURE_ID x64)',
        ]))


def run(args, cwd=None):
    return subprocess.run([sys.executable, str(TOOL), *args], cwd=cwd, text=True, capture_output=True)

with tempfile.TemporaryDirectory() as tmp:
    base = Path(tmp)
    for version in ['3.29.0', '4.4.0']:
        build = base / f'build {version}'
        compiler = 'C:/Program Files/Microsoft Visual Studio/VC/Tools/MSVC/14.44/bin/Hostx64/x64'
        write_layout(build, version, compiler)
        result = run(['--build-dir', str(build), '--expected-c-id', 'MSVC', '--expected-cxx-id', 'MSVC', '--expected-compiler-path-regex', r'Hostx64/x64', '--forbidden-compiler-path-regex', r'(mingw|msys|strawberry|Hostx86/x86)', '--expected-architecture', 'x64', '--expected-generator', 'Ninja', '--expected-build-type', 'Debug'])
        assert result.returncode == 0, result.stderr + result.stdout
    bad = base / 'bad'
    write_layout(bad, '4.4.0', 'C:/msys64/mingw64/bin')
    result = run(['--build-dir', str(bad), '--expected-c-id', 'MSVC', '--expected-cxx-id', 'MSVC', '--forbidden-compiler-path-regex', r'(mingw|msys|strawberry)'])
    assert result.returncode != 0, result.stdout
print('OK: CMake toolchain validator covers CMake 3.x, CMake 4.x, spaces, and forbidden paths.')
