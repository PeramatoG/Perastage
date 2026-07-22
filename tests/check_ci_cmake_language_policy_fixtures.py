#!/usr/bin/env python3
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--bash', required=True)
args = parser.parse_args()
BASH = args.bash
SCRIPT = ROOT / 'tests/check_ci_cmake_language_policy.sh'
needed = ['.github/workflows/ci-tests.yml', '.github/workflows/windows-installer.yml', '.github/workflows/linux-installer.yml', '.github/workflows/macos-installer.yml', '.github/workflows/macos-15-manual-installer.yml', '.github/workflows/arch-package.yml']

with tempfile.TemporaryDirectory() as tmp:
    repo = Path(tmp) / 'repo'
    for rel in needed:
        target = repo / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / rel, target)
    (repo / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.25)\nif(POLICY CMP0177)\ncmake_policy(SET CMP0177 NEW)\nendif()\nproject(Perastage LANGUAGES C CXX)\n')
    (repo / 'tests').mkdir()
    (repo / 'tests/CMakeLists.txt').write_text('add_test(NAME Example COMMAND example)\n')
    (repo / 'vcpkg/buildtrees/liblzma').mkdir(parents=True)
    (repo / 'vcpkg/buildtrees/liblzma/CMakeLists.txt').write_text('enable_language(C)\n')
    env = {**os.environ, 'PERASTAGE_POLICY_ROOT': str(repo)}
    ok = subprocess.run([BASH, str(SCRIPT)], env=env, text=True, capture_output=True)
    assert ok.returncode == 0, ok.stderr + ok.stdout
    (repo / 'cmake').mkdir()
    (repo / 'cmake/CMakeLists.txt').write_text('enable_language(C)\n')
    bad = subprocess.run([BASH, str(SCRIPT)], env=env, text=True, capture_output=True)
    assert bad.returncode != 0 and 'Nested enable_language()' in (bad.stderr + bad.stdout), bad.stderr + bad.stdout
print('OK: CMake language policy fixtures cover first-party failures and vcpkg exclusions.')
