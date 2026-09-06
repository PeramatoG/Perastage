#!/usr/bin/env python3
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--bash', required=True)
args = parser.parse_args()
BASH = args.bash
SCRIPT = ROOT / 'tests/check_windows_ninja_x64_policy.sh'
NEEDED = [
    'CMakePresets.json',
    'setup_windows.ps1',
    'scripts/windows/PerastageWindowsBootstrap.ps1',
    'scripts/windows/PerastageWindowsBootstrap.psm1',
]


def write_policy_root(repo: Path) -> None:
    for rel in NEEDED:
        target = repo / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / rel, target)
    (repo / 'tests').mkdir(exist_ok=True)


def run_policy(repo: Path) -> subprocess.CompletedProcess[str]:
    env = {**os.environ, 'PERASTAGE_POLICY_ROOT': str(repo), 'PERASTAGE_TEST_PYTHON': sys.executable}
    return subprocess.run([BASH, str(SCRIPT)], env=env, text=True, capture_output=True)


with tempfile.TemporaryDirectory() as tmp:
    repo = Path(tmp) / 'repo'
    repo.mkdir()
    write_policy_root(repo)
    for excluded in ['.git', '.vcpkg-cache', '.vcpkg-root', '.tools', 'build', 'build-windows-debug', 'cmake-build-debug', 'out', 'third_party', 'vcpkg', 'vcpkg_installed']:
        generated = repo / excluded / 'generated'
        generated.mkdir(parents=True, exist_ok=True)
        (generated / 'ignored.txt').write_text('local-' + 'win-debug-ninja\n')
    ok = run_policy(repo)
    assert ok.returncode == 0, ok.stderr + ok.stdout
    assert 'Checked ' in ok.stdout and 'first-party files' in ok.stdout, ok.stderr + ok.stdout

    presets = repo / 'CMakePresets.json'
    portable_presets = presets.read_text(encoding='utf-8')
    presets.write_text(
        portable_presets.replace(
            '${sourceDir}/cmake/PerastageWindowsVcpkgToolchain.cmake',
            'D:/fixture-vcpkg/scripts/buildsystems/vcpkg.cmake',
        ),
        encoding='utf-8',
    )
    fixed_root = run_policy(repo)
    assert fixed_root.returncode != 0, fixed_root.stderr + fixed_root.stdout
    presets.write_text(portable_presets, encoding='utf-8')

    first_party = repo / 'cmake' / 'policy.txt'
    first_party.parent.mkdir(parents=True, exist_ok=True)
    first_party.write_text('local-' + 'win-debug-ninja\n')
    bad = run_policy(repo)
    assert bad.returncode != 0 and 'generated local Windows presets' in (bad.stderr + bad.stdout), bad.stderr + bad.stdout

print('OK: Windows Ninja x64 policy prunes generated roots and still detects first-party local preset references.')
