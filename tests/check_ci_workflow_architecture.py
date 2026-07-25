#!/usr/bin/env python3
from pathlib import Path
import json
import re
import subprocess

WORKFLOWS = Path('.github/workflows')
for workflow in WORKFLOWS.glob('*.yml'):
    subprocess.run(['ruby', '-e', "require 'yaml'; YAML.load_file(ARGV[0])", str(workflow)], check=True)



# GitHub Packages is a second, centrally configured cache layer with one trusted writer.
all_workflows = {path.name: path.read_text() for path in WORKFLOWS.glob('*.yml')}
writers = [name for name, text in all_workflows.items() if re.search(r'packages:\s*write', text)]
assert writers == ['vcpkg-binary-cache.yml'], f'only the warming workflow may publish packages: {writers}'
assert all('pull_request_target' not in text for text in all_workflows.values())
assert all(not re.search(r'secrets\.[A-Z0-9_]*(?:PAT|PERSONAL_ACCESS_TOKEN)', text, re.IGNORECASE) for text in all_workflows.values())
remote = all_workflows['vcpkg-binary-cache.yml']
prerequisites = Path('.github/scripts/install_vcpkg_build_prerequisites.sh').read_text()
for needle in ['contents: read', 'packages: write', 'workflow_dispatch:', 'branches: [main]', '--mode readwrite', 'x64-windows', 'x64-linux', 'arm64-osx']:
    assert needle in remote, f'warming workflow is missing {needle}'
for needle in ['concurrency:', 'group: perastage-vcpkg-binary-cache', 'cancel-in-progress: false']:
    assert needle in remote, f'warming workflow must serialize package writers without cancellation: {needle}'
for needle in ['preflight:', 'needs: preflight', 'github.ref', 'refs/heads/main',
               'Require the trusted main branch', 'ref: ${{ github.sha }}']:
    assert needle in remote, f'warming workflow must enforce trusted main checkout: {needle}'
assert remote.index('preflight:') < remote.index('  warm:')
for needle in ['Resolve macOS SDK cache identity', 'identity=${sdk_identity}',
               'MACOS_SDK_PATH=${current_sdk_path}', 'MACOS_SDK_REALPATH=${current_sdk_realpath}',
               'VCPKG_CACHE_SCOPE=sdk-${sdk_identity}', 'macos_sdk_cache_guard.py',
               '--current-sdk "$MACOS_SDK_PATH"']:
    assert needle in remote, f'warming workflow is missing macOS cache boundary: {needle}'
assert remote.index('Guard restored macOS SDK metadata') < remote.index('Install and publish vcpkg packages')
assert 'VCPKG_CACHE_SCOPE: default' in remote, 'Windows and Ubuntu warming keys must retain the default scope'
assert '${{ matrix.triplet }}-${{ env.VCPKG_CACHE_SCOPE }}-' in remote
assert 'job.status' not in remote, 'job success must not be reported as independent publication verification'
assert '--install-outcome "${{ steps.vcpkg-install.outcome }}"' in remote
assert 'not independently verified' in remote
assert '.github/scripts/install_vcpkg_build_prerequisites.sh linux' in remote
assert '.github/scripts/install_vcpkg_build_prerequisites.sh macos' in remote
linux_packages = [
    'build-essential', 'cmake', 'ninja-build', 'pkg-config', 'autoconf',
    'automake', 'libtool', 'libx11-dev', 'libxi-dev', 'libxtst-dev',
    'libxrender-dev', 'libgtk-3-dev', 'libglib2.0-dev', 'libsecret-1-dev',
    'libpango1.0-dev', 'libatk1.0-dev', 'libcairo2-dev',
    'libgdk-pixbuf-2.0-dev', 'libxkbcommon-dev', 'libgl1-mesa-dev',
    'libglu1-mesa-dev', 'mono-complete',
]
for package in linux_packages:
    assert package in prerequisites, f'shared Linux vcpkg prerequisites are missing {package}'
for tool in ['autoconf', 'autoconf-archive', 'automake', 'gettext', 'libtool', 'ninja']:
    assert re.search(rf'brew install[^\n]*\b{re.escape(tool)}\b', prerequisites), f'shared macOS vcpkg prerequisites are missing {tool}'
assert 'command -v mono >/dev/null || brew install mono' in prerequisites
diagnostic_section = remote[remote.index('Upload vcpkg failure diagnostics'):remote.index('Save vcpkg downloads cache')]
for path in ['out/ci-logs/**', 'vcpkg/buildtrees/**/*.log', 'vcpkg/buildtrees/**/config-*.txt',
             'vcpkg/buildtrees/**/issue_body.md', '.vcpkg-cache/installed/vcpkg/issue_body.md']:
    assert path in diagnostic_section, f'warming failure diagnostics are missing {path}'
for exclusion in ['!**/NuGet.Config', '!**/*token*', '!**/*credential*', '!**/*authorization*']:
    assert exclusion in diagnostic_section, f'warming diagnostics must exclude credential material: {exclusion}'
install_section = remote[remote.index('Install and publish vcpkg packages'):remote.index('Upload vcpkg failure diagnostics')]
assert 'continue-on-error' not in install_section and '|| true' not in install_section
assert '-SkipDuplicate' not in remote, 'the pinned vcpkg publisher must not have failures broadly suppressed'
for forbidden in ['cmake --build', 'ctest', 'Configure CMake', 'pull_request_target']:
    assert forbidden not in remote, f'warming workflow must not contain {forbidden}'
consumers = ['ci-tests.yml', 'windows-installer.yml', 'linux-installer.yml', 'macos-installer.yml', 'macos-15-manual-installer.yml']
for name in consumers:
    text = all_workflows[name]
    assert 'packages: read' in text and 'packages: write' not in text
    assert '--mode read' in text and 'setup_vcpkg_github_packages.py' in text
for name in ['main-patch-test-build.yml', 'minor-draft-release.yml', 'compatibility-builds.yml']:
    assert 'packages: read' in all_workflows[name], f'{name} must grant reusable consumers package read access'
helper = Path('.github/scripts/setup_vcpkg_github_packages.py').read_text()
assert 'https://nuget.pkg.github.com/PeramatoG/index.json' in helper
assert 'https://github.com/PeramatoG/Perastage' in helper
assert 'defaultPushSource={FEED_URL}' in helper
assert '["setApiKey", token, "-Source", FEED_URL' in helper
assert 'ET.parse(config)' in helper
assert 'validate_config(config, args.mode)' in helper
assert 'validation.stdout' not in helper, 'structural validation must not depend on human-oriented NuGet output'
assert '["list", "-Source"' not in helper, 'setup must not query packages to validate NuGet configuration'
assert helper.index('files,{Path(args.local_cache).resolve()},readwrite') < helper.index('nugetconfig')
assert sum(text.count('nuget.exe') for text in all_workflows.values()) == 0, 'NuGet setup must remain centralized'
assert 'arch-package.yml' not in consumers and '--mode read' not in all_workflows['arch-package.yml'], 'Arch must remain local-only'

# vcpkg caches must be restored and explicitly saved before later build/test/package steps can fail.
VCPKG_WORKFLOWS = ['ci-tests.yml', 'windows-installer.yml', 'linux-installer.yml', 'macos-installer.yml', 'macos-15-manual-installer.yml', 'arch-package.yml']
for name in VCPKG_WORKFLOWS:
    text = (WORKFLOWS / name).read_text()
    assert 'actions/cache@v5' not in text, f'{name} must not use automatic post-job vcpkg cache saves'
    assert 'actions/cache/restore@v5' in text and 'actions/cache/save@v5' in text, f'{name} must use explicit cache restore/save actions'
    assert "hashFiles('vcpkg.json', 'vcpkg-configuration.json')" in text, f'{name} must key vcpkg caches from dependency inputs only'
    assert not re.search(r"hashFiles\([^)]*\.github/workflows/[^)]*\)", text), f'{name} must not hash workflow files into vcpkg keys'
    assert 'vcpkg-downloads-v3-' in text and 'vcpkg-compiled-v3-' in text, f'{name} must use the documented v3 vcpkg cache schema'
    assert '.vcpkg-cache/downloads' in text, f'{name} must keep downloads in a separate cache'
    assert '.vcpkg-cache/installed' in text or '.vcpkg-cache\\installed' in text, f'{name} must cache the installed vcpkg tree'
    assert '.vcpkg-cache/packages' in text or '.vcpkg-cache\\packages' in text, f'{name} must cache the packages tree'
    assert '.vcpkg-cache/binary' in text or '.vcpkg-cache\\binary' in text, f'{name} must cache the file-based binary cache'
    assert 'VCPKG_BINARY_SOURCES: clear;files,' in text and ',readwrite' in text, f'{name} must preserve the local vcpkg binary cache'
    assert 'steps.vcpkg-cache.outputs.cache-primary-key' in text, f'{name} must save the compiled cache with the restore primary key'
    assert 'write_vcpkg_cache_summary.py' in text, f'{name} must summarize vcpkg cache behavior'
    assert not re.search(r'vcpkg-(?:downloads|compiled)-v3[^\n]*(?:debug|release)', text, re.IGNORECASE), f'{name} must not split ABI-compatible vcpkg caches by Debug/Release labels'
    for cache_block in re.findall(r'uses: actions/cache/(?:restore|save)@v5[\s\S]{0,360}', text):
        assert not re.search(r'(?:^|\n)\s*(?:build|out|CTest|Testing/Temporary)(?:/|\\|$)', cache_block), f'{name} must not add build or test outputs to vcpkg cache paths'
    install_pos = text.index('vcpkg_install_retry.py')
    save_pos = text.index('Save vcpkg installed packages and binary archives', install_pos)
    later_needles = ['Configure CMake', 'Configure Debug tests', 'Configure Windows Debug tests', 'Configure macOS Debug tests', 'Build Arch package', 'Build project', 'Run complete CTest suite']
    later_positions = [text.find(needle, save_pos) for needle in later_needles if text.find(needle, save_pos) != -1]
    assert later_positions and save_pos < min(later_positions), f'{name} must save compiled vcpkg cache before configure/build/test/package steps'

ci_text = (WORKFLOWS / 'ci-tests.yml').read_text()
win_installer = (WORKFLOWS / 'windows-installer.yml').read_text()
linux_installer = (WORKFLOWS / 'linux-installer.yml').read_text()
assert 'vcpkg-compiled-v3-${{ runner.os }}-${{ runner.arch }}-x64-windows-default-' in ci_text and 'vcpkg-compiled-v3-${{ runner.os }}-${{ runner.arch }}-x64-windows-default-' in win_installer
assert 'vcpkg-compiled-v3-${{ runner.os }}-${{ runner.arch }}-x64-linux-default-' in ci_text and 'vcpkg-compiled-v3-${{ runner.os }}-${{ runner.arch }}-x64-linux-default-' in linux_installer
assert 'arm64-osx-sdk-${{ steps.macos-sdk.outputs.identity }}' in ci_text, 'macOS Debug CI must include the resolved SDK/Xcode identity'
assert 'arm64-osx-macos26-xcode26-' in (WORKFLOWS / 'macos-installer.yml').read_text(), 'macOS 26 installer must keep an SDK/Xcode cache boundary'
assert 'arm64-osx-macos15-deployment-${{ env.MACOSX_DEPLOYMENT_TARGET }}-' in (WORKFLOWS / 'macos-15-manual-installer.yml').read_text(), 'macOS 15 installer must keep deployment target cache boundary'
assert 'x64-linux-arch-' in (WORKFLOWS / 'arch-package.yml').read_text(), 'Arch packaging must remain isolated from Ubuntu-compatible Linux caches'
assert ci_text.count('.github/scripts/install_vcpkg_build_prerequisites.sh linux') == 1
assert ci_text.count('.github/scripts/install_vcpkg_build_prerequisites.sh macos') == 1

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
    for needle in ['Read vcpkg baseline', 'get_vcpkg_baseline.py vcpkg.json', 'Restore vcpkg downloads', 'Restore vcpkg installed packages and binary archives', 'Bootstrap vcpkg', 'vcpkg_install_retry.py', '-DVCPKG_MANIFEST_MODE=OFF', '-DBUILD_TESTING=ON', 'PERASTAGE_ENABLE_COMPILER_CACHE=ON', 'write_cmake_compiler_cache_init.py']:
        assert needle in text, f'{platform} Debug is missing {needle}'
    assert text.index('Prepare vcpkg and diagnostics directories') < text.index('Bootstrap vcpkg'), f'{platform} must create vcpkg directories before bootstrap'
    assert 'VCPKG_DOWNLOADS' in text and 'VCPKG_INSTALLED' in text and 'VCPKG_PACKAGES' in text and 'VCPKG_BINARY_CACHE' in text, f'{platform} must prepare all vcpkg directories'
    assert 'vcpkg-compiled-v3-' in text, f'{platform} installed cache key must use the shared v3 compiled schema'
    assert ('run_and_log.py --log' in text or '--output-log' in text) and 'ctest-' in text, f'{platform} build and test logs must be captured'

linux = sections['linux']
for needle in ['-DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"', '-DVCPKG_TARGET_TRIPLET=x64-linux', '-DVCPKG_INSTALLED_DIR="$VCPKG_INSTALLED"', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON', 'compile_commands.json was not generated', 'es_ES.UTF-8', 'zh_CN.UTF-8', 'xdpyinfo', 'xvfb-run -a', '--output-junit', '--verbose', 'ctest-inventory-linux-debug.txt', 'ctest-linux-debug-results.json', 'ci-linux-debug-test-results']:
    assert needle in linux, f'Linux Debug is missing {needle}'
assert 'summarize_ctest_results.py' in ci, 'CI Debug must produce compact result summaries'
assert 'LastTestsDisabled.log' in ci, 'CI Debug test-result artifacts must retain disabled-test diagnostics when present'
assert 'wxwidgets' not in linux.lower(), 'Linux Debug must not install system wxWidgets packages'

windows = sections['windows']
for needle in ['$env:GITHUB_ENV', '$env:GITHUB_PATH', 'PERASTAGE_PYTHON', 'Get-Command python', 'INCLUDE', 'LIBPATH', 'VSCMD_ARG_HOST_ARCH', 'VSCMD_ARG_TGT_ARCH', 'validate_cmake_toolchain.py', '--expected-c-id MSVC', '--expected-cxx-id MSVC', '--interactive-debug-mode 0', '--timeout 120', '--output-junit', 'timeout-minutes: 180', 'ctest-inventory-windows-debug.txt', 'ctest-windows-debug-results.json', 'ci-windows-debug-test-results']:
    assert needle in windows, f'Windows Debug environment persistence is missing {needle}'
assert windows.index('Persist Visual Studio Hostx64 x64 environment') < windows.index('Configure Windows Debug tests') < windows.index('Build Windows Debug tests')

macos = sections['macos']
for needle in ['sdk-${{ steps.macos-sdk.outputs.identity }}', 'xcrun --sdk macosx --show-sdk-path', '-DCMAKE_OSX_SYSROOT="$current_sdk_path"', '--output-junit', 'ctest-inventory-macos-debug.txt', 'ctest-macos-debug-results.json', 'ci-macos-debug-test-results']:
    assert needle in macos, f'macOS Debug is missing {needle}'

sccache_action = 'mozilla-actions/sccache-action@9e7fa8a12102821edf02ca5dbea1acd0f89a2696 # v0.0.10'
assert ci.count(sccache_action) == 3, 'each Debug platform must use the reviewed sccache action commit'
assert ci.count('version: v0.15.0') == 3, 'each Debug platform must pin sccache v0.15.0'
for platform, text in sections.items():
    for needle in ['SCCACHE_GHA_ENABLED: ${{ needs.resolve-source.outputs.sccache_gha_enabled }}', 'SCCACHE_BASEDIRS:',
                   'SCCACHE_DIR:', 'SCCACHE_LOCAL_RW_MODE: READ_WRITE', 'SCCACHE_CACHE_SCOPE:',
                   'SCCACHE_IGNORE_SERVER_IO_ERROR: "1"', 'perastage-ci-debug-v1-', '--zero-stats',
                   '--show-stats --stats-format json', 'write_sccache_summary.py', '--expected-launcher',
                   'write_cmake_compiler_cache_init.py', ' -C ']:
        assert needle in text, f'{platform} Debug sccache policy is missing {needle}'
    configure_name = {'linux': 'Configure Debug tests', 'windows': 'Configure Windows Debug tests', 'macos': 'Configure macOS Debug tests'}[platform]
    assert text.index('Install vcpkg packages') < text.index('Set up sccache') < text.index(configure_name), f'{platform} must install dependencies before starting sccache'
    assert 'SCCACHE_GHA_ENABLED' in text[text.index('Define '):text.index('Set up sccache')], f'{platform} must gate the persistent namespace on GHA enablement'
assert 'pull_request_target:' not in ci
assert 'SCCACHE_GHA_RW_MODE' not in ci and 'mode=READ_ONLY' not in ci and 'mode=READ_WRITE' not in ci
assert 'resolve_sccache_scope.py' in ci and "--event '${{ github.event_name }}'" in ci and "--github-ref '${{ github.ref }}'" in ci
assert '--source-sha "$sha"' in ci and '--trusted-main-sha "$trusted_main_sha"' in ci
assert 'git rev-parse refs/remotes/origin/main' in ci
assert '--policy-default-cmp0141 NEW' in windows and '--msvc-debug-information-format Embedded' in windows
assert 'validate_msvc_compile_commands.py' in windows
assert 'cmake-windows-debug-initial-cache.cmake' in windows and '-C "$env:CI_LOG_DIR\\cmake-windows-debug-initial-cache.cmake"' in windows
assert '$sccacheProgram' not in windows and '$cLauncher' not in windows and '$cxxLauncher' not in windows
assert windows.index('Set up sccache') < windows.index('Resolve Windows sccache executable') < windows.index('Initialize Windows sccache statistics')
for needle in ['Get-Command sccache.exe -ErrorAction Stop', 'Test-Path -LiteralPath $sccacheExecutable -PathType Leaf',
               ".EndsWith('.exe', [StringComparison]::OrdinalIgnoreCase)",
               'PERASTAGE_SCCACHE_EXECUTABLE=$sccacheExecutable', '$env:GITHUB_ENV']:
    assert needle in windows, f'Windows sccache executable resolution is missing {needle}'
for operation in ['--version', '--start-server', '--zero-stats', '--expected-launcher',
                  '--show-stats', '--show-stats --stats-format json']:
    assert re.search(rf'PERASTAGE_SCCACHE_EXECUTABLE[^\n]*{re.escape(operation)}|{re.escape(operation)}[^\n]*PERASTAGE_SCCACHE_EXECUTABLE', windows), f'Windows must use the resolved executable for {operation}'
assert not re.search(r'write_cmake_compiler_cache_init\.py[^\n]+\$env:SCCACHE_PATH', windows)
assert '$sccacheExecutable = $env:PERASTAGE_SCCACHE_EXECUTABLE' in windows and '--launcher "$sccacheExecutable"' in windows
assert '$env:SCCACHE_PATH --' not in windows and '--expected-launcher "$env:SCCACHE_PATH"' not in windows
assert 'write_cmake_compiler_cache_init.py --launcher "$SCCACHE_PATH"' in linux
assert 'write_cmake_compiler_cache_init.py --launcher "$SCCACHE_PATH"' in macos
configure = windows[windows.index('      - name: Configure Windows Debug tests'):windows.index('      - name: Validate Windows Debug toolchain')]
assert 'validate_cmake_toolchain.py' not in configure and 'validate_msvc_compile_commands.py' not in configure
for forbidden in ['-DCMAKE_C_COMPILER=', '-DCMAKE_CXX_COMPILER=', '-DBASH_EXECUTABLE=',
                  '-DCMAKE_POLICY_DEFAULT_CMP0141=', '-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=']:
    assert forbidden not in configure, f'Windows configure must transport {forbidden} through the initial cache'
for option in ['--c-compiler', '--cxx-compiler', '--bash-executable', '--policy-default-cmp0141 NEW',
               '--msvc-debug-information-format Embedded']:
    assert option in windows, f'Windows initial cache is missing {option}'
assert windows.index('Configure Windows Debug tests') < windows.index('Validate Windows Debug toolchain and sccache launcher') < windows.index('Validate Windows Debug compile flags') < windows.index('Build Windows Debug tests')
assert 'cmake-toolchain-validation-windows-debug.log' in windows and 'msvc-compile-flags-windows-debug.log' in windows
assert 'build-windows-debug/compile_commands.json' in windows, 'Windows failure diagnostics must retain the compile database'
assert '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON' in macos and 'macOS Debug compile_commands.json was not generated' in macos
for metric in ['Cache writes', 'Cache read errors', 'Cache write errors', 'Requests executed', 'Compilation failures']:
    assert metric in Path('.github/scripts/write_sccache_summary.py').read_text(), f'sccache summary must report {metric}'
assert 'SCCACHE_RECACHE' not in ci and 'ACTIONS_RUNTIME_TOKEN' not in ci and 'ACTIONS_RESULTS_URL' not in ci
for path in WORKFLOWS.glob('*.yml'):
    if path.name != 'ci-tests.yml':
        assert 'sccache-action' not in path.read_text(), f'{path.name} must remain outside PR 3A sccache scope'

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


macos_symbol_uploads = {
    'macos-15-manual-installer.yml': ('Perastage-macos15-symbols', 'out/symbols/macos15'),
    'macos-installer.yml': ('Perastage-macos26-symbols', 'out/symbols/macos26'),
}
for name, (artifact_name, parent_path) in macos_symbol_uploads.items():
    text = (WORKFLOWS / name).read_text()
    pattern = rf'name: {re.escape(artifact_name)}\s+path: {re.escape(parent_path)}\s+if-no-files-found: error'
    assert re.search(pattern, text), f'{name} must upload the parent dSYM symbol directory with if-no-files-found: error'
    assert f'path: {parent_path}/Perastage.dSYM' not in text, f'{name} must not upload the dSYM bundle as the artifact root'

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
assert 'validate_final_release_assets.py' in minor and 'release-provenance.json' in minor
assert 'symbol-files.txt' not in minor, 'final symbol archive must not be a path list only'
assert re.search(r'publish-release:[\s\S]+needs: \[resolve-release, stage-release-commit, validate-release-assets\]', minor)
assert minor.find('git tag -a') > minor.find('validate-release-assets'), 'final tag must be created after asset validation'
assert 'git push --atomic origin' in minor, 'normal publication must use atomic branch and tag push'
assert '"${RELEASE_SHA}:refs/heads/main"' in minor and '"refs/tags/${NEW_TAG}:refs/tags/${NEW_TAG}"' in minor
assert 'git push origin "$RELEASE_SHA":main' not in minor, 'normal publication must not push main before tagging'
assert 'git config user.name "github-actions[bot]"' in minor and minor.find('git config user.name "github-actions[bot]"', minor.find('publish-release:')) < minor.find('git tag -a', minor.find('publish-release:'))
assert 'refs/tags/${NEW_TAG}^{commit}' in minor, 'annotated tags must be compared by peeled commit target'
assert 'Unsupported publication state' in minor and 'Git publication is already complete' in minor
assert 'TEMP_REF="refs/heads/automation/release-${NEW_TAG}-${GITHUB_RUN_ID}"' in minor, 'minor release temp ref must be fully qualified under refs/heads/automation/release-'
assert 'TEMP_REF="automation/release-' not in minor, 'minor release must not use a short temporary branch name'
assert re.search(r'echo\s+"Creating temporary release ref:\s+\$\{TEMP_REF\}"', minor), 'staging must log the exact temporary ref before push'
assert re.search(r'git\s+push\s+origin\s+"HEAD:\$\{TEMP_REF\}"', minor), 'staging must push detached HEAD with an explicit fully qualified refspec'
assert 'git push origin HEAD:"$TEMP_REF"' not in minor, 'staging must not use the old ambiguous short destination syntax'
assert re.search(r'git\s+push\s+origin\s+":\$\{TEMP_REF\}"', minor), 'successful publication must delete the exact temporary refspec'
assert 'git push origin --delete "$TEMP_REF"' not in minor, 'minor release must not delete temporary refs through short-name guessing'
cleanup = minor[minor.index('  cleanup-temp-ref:'):]
assert 'if: ${{ always() && needs.resolve-release.outputs.dry_run != \'true\' }}' in cleanup
assert 'actions/checkout@v6' in cleanup
assert re.search(r'git\s+ls-remote\s+--exit-code\s+origin\s+"\$\{TEMP_REF\}"', cleanup), 'fallback cleanup must check the exact fully qualified temporary ref'
assert re.search(r'git\s+push\s+origin\s+":\$\{TEMP_REF\}"', cleanup), 'fallback cleanup must delete the exact temporary refspec'
assert not re.search(r'automation/\*|refs/heads/automation/\*|for\s+.+automation|git\s+branch\s+-r[\s\S]+automation', cleanup), 'fallback cleanup must not enumerate or wildcard-delete automation branches'
assert 'git push origin --delete "$TEMP_REF" || true' not in cleanup


recover = (WORKFLOWS / 'recover-minor-release.yml').read_text()
assert 'name: Recover Validated Minor Release' in recover
assert 'workflow_dispatch:' in recover and 'source_run_id:' in recover and 'dry_run:' in recover
assert 'contents: write' in recover and 'actions: read' in recover
assert 'perastage-minor-release-recovery' in recover
assert 'c857665b99aacf9f466edd4416584dfb56ac1a1f' in recover and 'default: 1.5.0' in recover
assert 'git fetch origin "$RELEASE_SHA"' in recover, 'recovery must fetch the exact supplied SHA'
assert 'git show "${RELEASE_SHA}:VERSION"' in recover, 'recovery must read VERSION from the exact supplied commit'
assert 'git merge-base --is-ancestor "$RELEASE_SHA" origin/main' in recover
assert 'git push --atomic' not in recover and 'refs/heads/main' not in recover and ':main' not in recover, 'recovery must never update main'
assert 'Perastage-validated-release-assets' in recover and 'gh run download "$SOURCE_RUN_ID" --name Perastage-validated-release-assets' in recover
assert 'validate_final_release_assets.py' in recover and '--validate-provenance' in recover
assert 'if [ "$DRY_RUN" = true ]; then' in recover
assert recover.find('if [ "$DRY_RUN" = true ]; then') < recover.find('git tag -a "$TAG"'), 'dry run must exit before writes'
assert 'git config user.name "github-actions[bot]"' in recover and recover.find('git config user.name "github-actions[bot]"') < recover.find('git tag -a "$TAG"')
assert 'gh release view "$TAG"' in recover and 'gh release upload "$TAG" "${public_assets[@]}" --clobber' in recover

contract = json.loads(Path('.github/release-artifact-contract.json').read_text())
patterns = contract['packages']
for pattern in patterns.values():
    assert pattern in minor, f'collector is missing package pattern {pattern}'
for artifact in contract['artifacts'].values():
    found = any(artifact in (WORKFLOWS / name).read_text() for name in builders) or artifact in minor
    assert found, f'artifact contract name is not produced or consumed: {artifact}'

print('OK: GitHub Actions architecture policies are enforced.')
