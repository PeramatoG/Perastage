#!/usr/bin/env python3
from pathlib import Path
import json
import re
import sys

import subprocess

WORKFLOWS = Path('.github/workflows')
for workflow in WORKFLOWS.glob('*.yml'):
    subprocess.run(['ruby', '-e', "require 'yaml'; YAML.load_file(ARGV[0])", str(workflow)], check=True)

ci = (WORKFLOWS / 'ci-tests.yml').read_text()
for needle in ['name: CI Debug Tests', 'pull_request:', 'workflow_call:', 'CMAKE_BUILD_TYPE=Debug', '-DBUILD_TESTING=ON', 'cancel-in-progress: true', '-host_arch=x64 -arch=x64', 'VCPKG_TARGET_TRIPLET=x64-windows']:
    assert needle in ci, f'ci-tests.yml is missing {needle}'
assert '-DNDEBUG' in ci and 'must not compile with NDEBUG' in ci
assert 'Refusing non-MSVC compiler' in ci and '(?i)mingw|msys|strawberry' in ci, 'Windows Debug CI must reject MinGW, MSYS, and Strawberry tools'

builders = ['windows-installer.yml','linux-installer.yml','macos-installer.yml','macos-15-manual-installer.yml','arch-package.yml']
for name in builders:
    text = (WORKFLOWS / name).read_text()
    assert 'workflow_call:' in text and 'workflow_dispatch:' in text, f'{name} must remain reusable and manual'
    assert "ref: ${{ inputs.source_ref != '' && inputs.source_ref || github.ref }}" in text, f'{name} must checkout source_ref'
    assert 'PERASTAGE_REQUIRE_SECURE_CREDENTIAL_STORE=ON' in text, f'{name} must require secure store'
    assert 'PERASTAGE_ENABLE_COMPILER_CACHE=OFF' in text, f'{name} must disable uncontrolled compiler cache'
    assert '-DBUILD_TESTING=ON' not in text, f'{name} must not configure tests'
    assert 'release-gate' not in text, f'{name} must not run release-gate tests'
for name in ['linux-installer.yml','macos-installer.yml','macos-15-manual-installer.yml','windows-installer.yml']:
    text = (WORKFLOWS / name).read_text()
    assert 'CMAKE_BUILD_TYPE=Release' in text, f'{name} must be Release'
    assert '-DBUILD_TESTING=OFF' in text, f'{name} must disable tests'

main_patch = (WORKFLOWS / 'main-patch-test-build.yml').read_text()
assert 'name: Main Patch Release Artifacts' in main_patch
assert main_patch.count('uses: ./.github/workflows/windows-installer.yml') == 1
assert main_patch.count('uses: ./.github/workflows/linux-installer.yml') == 1
assert main_patch.count('uses: ./.github/workflows/macos-installer.yml') == 1
assert 'macos-15-manual-installer.yml' not in main_patch and 'arch-package.yml' not in main_patch and 'ci-tests.yml' not in main_patch

compat = (WORKFLOWS / 'compatibility-builds.yml').read_text()
assert 'name: Weekly Compatibility Packages' in compat and 'schedule:' in compat
assert compat.count('uses: ./.github/workflows/macos-15-manual-installer.yml') == 1
assert compat.count('uses: ./.github/workflows/arch-package.yml') == 1
assert 'windows-installer.yml' not in compat and 'linux-installer.yml' not in compat and 'macos-installer.yml' not in compat
assert 'git commit' not in compat and 'git push origin HEAD:main' not in compat and 'printf' not in compat, 'compatibility workflow must not mutate VERSION'

minor = (WORKFLOWS / 'minor-draft-release.yml').read_text()
for builder in ['windows-installer.yml','linux-installer.yml','macos-15-manual-installer.yml','macos-installer.yml','arch-package.yml']:
    assert builder in minor, f'minor release must require {builder}'
assert 'uses: ./.github/workflows/ci-tests.yml' in minor
assert 'validate-release-assets' in minor and 'publish-release' in minor
assert re.search(r'publish-release:[\s\S]+needs: \[resolve-release, stage-release-commit, validate-release-assets\]', minor)
assert minor.find('git tag -a') > minor.find('validate-release-assets'), 'final tag must be created after asset validation'
assert 'git push origin "$RELEASE_SHA":main' in minor and 'main moved' in minor
assert 'git push origin --delete "$TEMP_REF"' in minor

contract = json.loads(Path('.github/release-artifact-contract.json').read_text())
patterns = contract['packages']
for pattern in patterns.values():
    assert pattern in minor, f'collector is missing package pattern {pattern}'
for artifact in contract['artifacts'].values():
    found = any(artifact in (WORKFLOWS / name).read_text() for name in builders) or artifact in minor
    assert found, f'artifact contract name is not produced or consumed: {artifact}'

print('OK: GitHub Actions architecture policies are enforced.')
