#!/usr/bin/env python3
from pathlib import Path
import json
import re
import subprocess

WORKFLOWS = Path('.github/workflows')
for workflow in WORKFLOWS.glob('*.yml'):
    subprocess.run(['ruby', '-e', "require 'yaml'; YAML.load_file(ARGV[0])", str(workflow)], check=True)

ci = (WORKFLOWS / 'ci-tests.yml').read_text()
for needle in ['name: CI Debug Tests', 'pull_request:', 'workflow_call:', 'CMAKE_BUILD_TYPE=Debug', '-DBUILD_TESTING=ON', 'cancel-in-progress: true', '-host_arch=x64 -arch=x64', 'VCPKG_TARGET_TRIPLET=x64-windows']:
    assert needle in ci, f'ci-tests.yml is missing {needle}'
assert '-DNDEBUG' in ci and 'must not compile with NDEBUG' in ci
assert 'Refusing non-MSVC compiler' in ci and all(token in ci.lower() for token in ['mingw', 'msys', 'strawberry']), 'Windows Debug CI must reject MinGW, MSYS, and Strawberry tools'

sections = {
    'linux': ci[ci.index('  linux-debug:'):ci.index('\n  windows-debug:')],
    'windows': ci[ci.index('  windows-debug:'):ci.index('\n  macos-debug:')],
    'macos': ci[ci.index('  macos-debug:'):],
}
for platform, text in sections.items():
    for needle in ['Read vcpkg baseline', 'get_vcpkg_baseline.py vcpkg.json', 'Restore vcpkg downloads', 'Cache vcpkg installed packages and binary archives', 'Bootstrap vcpkg', 'vcpkg_install_retry.py', '-DVCPKG_MANIFEST_MODE=OFF', '-DBUILD_TESTING=ON', 'PERASTAGE_ENABLE_COMPILER_CACHE=OFF']:
        assert needle in text, f'{platform} Debug is missing {needle}'
    assert text.index('Prepare vcpkg and diagnostics directories') < text.index('Bootstrap vcpkg'), f'{platform} must create vcpkg directories before bootstrap'
    assert 'VCPKG_DOWNLOADS' in text and 'VCPKG_INSTALLED' in text and 'VCPKG_PACKAGES' in text and 'VCPKG_BINARY_CACHE' in text, f'{platform} must prepare all vcpkg directories'
    assert 'debug-v1' in text or 'debug-v2' in text or 'macos-26-xcode-26-debug-v1' in text, f'{platform} installed cache key must be Debug/toolchain scoped'
    assert ('run_and_log.py --log' in text or '--output-log' in text) and 'ctest-' in text, f'{platform} build and test logs must be captured'

linux = sections['linux']
for needle in ['-DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"', '-DVCPKG_TARGET_TRIPLET=x64-linux', '-DVCPKG_INSTALLED_DIR="$VCPKG_INSTALLED"', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON', 'compile_commands.json was not generated', 'ripgrep', 'xvfb', 'locales', 'es_ES.UTF-8', 'zh_CN.UTF-8', 'xauth', 'x11-utils', 'xdpyinfo', 'xvfb-run -a', '--output-junit', '--verbose', 'ctest-inventory-linux-debug.txt', 'ctest-linux-debug-results.json', 'ci-linux-debug-test-results']:
    assert needle in linux, f'Linux Debug is missing {needle}'
assert 'summarize_ctest_results.py' in ci, 'CI Debug must produce compact result summaries'
assert 'LastTestsDisabled.log' in ci, 'CI Debug test-result artifacts must retain disabled-test diagnostics when present'
assert 'wxwidgets' not in linux.lower(), 'Linux Debug must not install system wxWidgets packages'

windows = sections['windows']
for needle in ['$env:GITHUB_ENV', '$env:GITHUB_PATH', 'PERASTAGE_PYTHON', 'Get-Command python', 'INCLUDE', 'LIBPATH', 'VSCMD_ARG_HOST_ARCH', 'VSCMD_ARG_TGT_ARCH', 'validate_cmake_toolchain.py', '--expected-c-id MSVC', '--expected-cxx-id MSVC', '--interactive-debug-mode 0', '--timeout 120', '--output-junit', 'timeout-minutes: 180', 'ctest-inventory-windows-debug.txt', 'ctest-windows-debug-results.json', 'ci-windows-debug-test-results']:
    assert needle in windows, f'Windows Debug environment persistence is missing {needle}'
assert windows.index('Persist Visual Studio Hostx64 x64 environment') < windows.index('Configure Windows Debug tests') < windows.index('Build Windows Debug tests')

macos = sections['macos']
for needle in ['debug-v2', 'xcrun --sdk macosx --show-sdk-path', '-DCMAKE_OSX_SYSROOT="$current_sdk_path"', '--output-junit', 'ctest-inventory-macos-debug.txt', 'ctest-macos-debug-results.json', 'ci-macos-debug-test-results']:
    assert needle in macos, f'macOS Debug is missing {needle}'

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
assert re.search(r'windows-installer:[\s\S]+needs: bump-version', main_patch)
assert re.search(r'linux-installer:[\s\S]+needs: bump-version', main_patch)
assert re.search(r'macos-installer:[\s\S]+needs: bump-version', main_patch)

compat = (WORKFLOWS / 'compatibility-builds.yml').read_text()
assert 'name: Weekly Compatibility Packages' in compat and 'schedule:' in compat
assert compat.count('uses: ./.github/workflows/macos-15-manual-installer.yml') == 1
assert compat.count('uses: ./.github/workflows/arch-package.yml') == 1
assert 'windows-installer.yml' not in compat and 'linux-installer.yml' not in compat and 'macos-installer.yml' not in compat
assert 'git commit' not in compat and 'git push origin HEAD:main' not in compat and 'printf' not in compat, 'compatibility workflow must not mutate VERSION'

minor = (WORKFLOWS / 'minor-draft-release.yml').read_text()
for builder in ['windows-installer.yml','linux-installer.yml','macos-15-manual-installer.yml','macos-installer.yml','arch-package.yml']:
    assert builder in minor, f'minor release must require {builder}'
assert 'uses: ./.github/workflows/ci-tests.yml' not in minor, 'minor package creation must not depend on Debug CI'
assert re.search(r'stage-release-commit:[\s\S]+needs: resolve-release', minor), 'stage-release-commit must depend only on release metadata resolution'
assert 'validate-release-assets' in minor and 'publish-release' in minor
assert 'python3 .github/scripts/assemble_debug_symbols.py' in minor
assert 'symbol-files.txt' not in minor, 'final symbol archive must not be a path list only'
assert re.search(r'publish-release:[\s\S]+needs: \[resolve-release, stage-release-commit, validate-release-assets\]', minor)
assert minor.find('git tag -a') > minor.find('validate-release-assets'), 'final tag must be created after asset validation'
assert 'git push origin "$RELEASE_SHA":main' in minor and 'main moved' in minor
assert 'git push origin --delete "$TEMP_REF"' in minor
cleanup = minor[minor.index('  cleanup-temp-ref:'):]
assert 'actions/checkout@v6' in cleanup and 'git ls-remote --exit-code --heads origin "$TEMP_REF"' in cleanup
assert 'git push origin --delete "$TEMP_REF" || true' not in cleanup

contract = json.loads(Path('.github/release-artifact-contract.json').read_text())
patterns = contract['packages']
for pattern in patterns.values():
    assert pattern in minor, f'collector is missing package pattern {pattern}'
for artifact in contract['artifacts'].values():
    found = any(artifact in (WORKFLOWS / name).read_text() for name in builders) or artifact in minor
    assert found, f'artifact contract name is not produced or consumed: {artifact}'

print('OK: GitHub Actions architecture policies are enforced.')
