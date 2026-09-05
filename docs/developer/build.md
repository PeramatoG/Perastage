# Build and Dependency Guide

This document covers baseline and advanced build behavior for Perastage. It is the detailed companion to the short installation section in `README.md`.

## Core Requirements

- CMake 3.21 or newer.
- C++20-capable compiler/toolchain.
- wxWidgets 3.3.1 or compatible development package.
- Required libraries:
  - wxWidgets
  - tinyxml2
  - OpenGL / GLU
  - GLEW
  - CURL
  - nanovg
  - PoDoFo
  - meshoptimizer
  - ZLIB
  - backward-cpp
  - mdns, when `PERASTAGE_ENABLE_MVR_XCHANGE_MDNS` is enabled

## Windows vcpkg dependency setup

Perastage keeps the root `vcpkg.json` manifest as the dependency source of truth for CI and for documenting the required packages. The normal local Windows workflow is intentionally classic vcpkg: set `VCPKG_ROOT` to a classic vcpkg checkout, install dependencies once into `$env:VCPKG_ROOT\installed\x64-windows`, and then configure with the shared Ninja presets. Visual Studio and CMake must not run an automatic vcpkg install during local configure.

The canonical local Windows presets are:

- `win-x64-debug-ninja`
- `win-x64-release-ninja`

Both presets use the schema-v3 top-level `toolchainFile` field to load `cmake/PerastageWindowsVcpkgToolchain.cmake`, plus `VCPKG_TARGET_TRIPLET=x64-windows`, `VCPKG_MANIFEST_MODE=OFF`, and `VCPKG_MANIFEST_INSTALL=OFF`. This repository-owned bootstrap selects a valid external checkout from `VCPKG_ROOT`, or from the standard user-wide `vcpkg.path.txt` descriptor when Visual Studio has injected its bundled root. It then normalizes `VCPKG_ROOT` for downstream tools and includes the external vcpkg toolchain. CMake therefore resolves already-installed packages from the external `installed/x64-windows` tree and does not print `-- Running vcpkg install` during a clean Visual Studio configure.

Install or repair dependencies manually before configuring if they are missing. A typical one-time command is:

```powershell
& "$env:VCPKG_ROOT\vcpkg.exe" install --triplet x64-windows wxwidgets[secretstore] gettext[tools] tinyxml2 curl glew zlib nanovg podofo meshoptimizer backward-cpp mdns
```

Gettext tools are build-time dependencies for localization catalog generation. On Windows they should resolve from `$env:VCPKG_ROOT\installed\x64-windows\tools\gettext\bin`. They are not Perastage runtime dependencies. Homebrew gettext is keg-only on macOS; add `$(brew --prefix gettext)/bin` to `PATH` before configuring CMake so `msgfmt`, `xgettext`, `msgmerge`, and `msgattrib` resolve consistently.

Use the setup script as a validator/build helper, not as an installer:

```powershell
cd C:\path\to\Perastage
$env:VCPKG_ROOT = 'D:\path\to\vcpkg'
.\setup_windows.ps1 -Configuration Debug -CleanBuild -SkipBuild
# Optional explicit Git Bash override:
.\setup_windows.ps1 -Configuration Debug -CleanBuild -SkipBuild -BashExecutable "C:\Program Files\Git\bin\bash.exe"
```

`setup_windows.ps1` resolves the checkout from explicit `-VcpkgRoot` first, a valid external `VCPKG_ROOT` second, and the standard user-wide integration descriptor third; it fails if none identifies a valid external checkout. It validates `vcpkg.exe`, `.vcpkg-root`, `scripts\buildsystems\vcpkg.cmake`, `installed\x64-windows`, representative package headers, gettext tools, and `wxUSE_SECRETSTORE`. Before invoking the shared preset it exports the resolved root as `VCPKG_ROOT`, so validation and CMake cannot select different installations. It also imports and validates an x64 MSVC environment and removes only the selected Perastage build directory when a stale incompatible CMake cache is detected. It does not clone vcpkg, bootstrap vcpkg, run vcpkg installs, generate `CMakeUserPresets.json`, create `.tools\vcpkg`, or create a repository-local `vcpkg_installed` tree.

If an older build was configured against wxWidgets without `secretstore`, manifest mode, another installed root, or an x86 compiler, rerun the script with `-CleanBuild` to delete only the selected Perastage build directory before reconfiguring. Deleting `.vs` or `build` does not require reinstalling packages, and deleting `$env:VCPKG_ROOT\installed` is not part of normal troubleshooting.


### Canonical local Windows x64 bootstrap

`setup_windows.ps1` is the canonical local Windows x64 Ninja entry point. A generic Visual Studio Developer PowerShell can expose Hostx86/x86 tools depending on how it was launched, so the script always imports `VsDevCmd.bat -host_arch=x64 -arch=x64` itself and verifies that `cl.exe`, `link.exe`, `VSCMD_ARG_HOST_ARCH`, and `VSCMD_ARG_TGT_ARCH` all describe Hostx64/x64 before CMake configure starts.

Git Bash does not need to be first on `PATH`. The setup script accepts an optional explicit override through `-BashExecutable` or the `BASH_EXECUTABLE` environment variable; a valid explicit Git Bash wins over automatic discovery. Without an override, the script resolves Git for Windows, derives `bash.exe` from that installation, rejects WSL/System32 and WindowsApps launchers, runs a non-login shell probe, and passes the resolved path to CMake as `-DBASH_EXECUTABLE=...`. If Git for Windows is missing or only a launcher is available, the expected failure is:

```text
Git Bash could not be resolved. Install Git for Windows or pass -DBASH_EXECUTABLE=<Git for Windows bash.exe>; WSL and WindowsApps bash launchers are not supported.
```

Use this clean Debug validation command after installing Visual Studio C++ tools, Git for Windows, Ninja, CMake, and classic vcpkg dependencies under `VCPKG_ROOT`:

```powershell
.\setup_windows.ps1 -Configuration Debug -CleanBuild -SkipBuild
```

After a successful configure, inspect the cache entries that prove the intended tools were selected:

```powershell
Select-String -Path build\win-x64-debug-ninja\CMakeCache.txt -Pattern '^(BASH_EXECUTABLE|CMAKE_C_COMPILER|CMAKE_CXX_COMPILER|VCPKG_TARGET_TRIPLET):'
```

Expected values are a Git-for-Windows `bash.exe`, MSVC compilers under `VC\Tools\MSVC\...\bin\Hostx64\x64`, and `VCPKG_TARGET_TRIPLET:STRING=x64-windows`.

## CMake presets strategy

`CMakePresets.json` is the canonical, version-controlled source for supported
local CMake configure and build behavior. Shared platform choices belong there;
setup scripts may prepare and validate an environment, but ultimately delegate
configuration and build semantics to those presets. CI and packaging workflows
may use explicit CMake configuration for their separate clean-environment and
artifact-building responsibilities.

```text
CMakePresets.json
CMakeUserPresets.json
```

The tracked presets intentionally provide these local workflows without aliases or
duplicate platform configurations:

- Windows x64 uses the `win-x64-*-ninja` configure presets and matching
  `win-*-build-ninja` build presets. `setup_windows.ps1` validates dependencies,
  imports the x64 MSVC environment, and then invokes those presets. The same
  shared presets appear when the repository folder is opened in Visual Studio.
- macOS Apple Silicon uses `mac-arm64-debug` / `mac-arm64-release` and their
  matching build presets. Ninja and a developer-provided `VCPKG_ROOT` are
  prerequisites, and secure credential-store support is required.
- Native Linux and WSL intentionally share the existing `wsl-x64-debug` /
  `wsl-x64-release` configure presets and matching build presets. Despite their
  historical `wsl` names, their Linux host condition and system-package model
  support both environments. Their `/mnt/c` ignore paths prevent a WSL build
  from accidentally discovering Windows vcpkg packages and are harmless on
  native Linux.

`CMakeUserPresets.json` is an optional, developer-owned extension and is ignored
by Git. Perastage does not generate or require it: setup scripts, CI, packaging,
and normal supported builds operate from `CMakePresets.json` alone, subject to
the documented external prerequisites. Use a user preset only for an
intentionally machine-specific environment value or personal build-directory
variant. Inherit a shared preset rather
than copying its configuration. For example, a developer whose private vcpkg
checkout is at the illustrative path below could create this untracked file:

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "developer-win-debug",
      "inherits": "win-x64-debug-ninja",
      "environment": {
        "VCPKG_ROOT": "D:/developer-example/vcpkg"
      }
    }
  ]
}
```

The schema version matches the repository's CMake 3.21 minimum and supports
configure-preset inheritance. Developer-specific paths belong only in this
ignored file and must not become required shared state. The supported Visual Studio folder workflow consumes `CMakePresets.json` directly. The former `CMakeSettings.json` duplicated Debug and Release configuration with another generator, while `CppProperties.json` duplicated C++20 and include information supplied by CMake and pointed at the Debug preset compile database. Repository, setup, CI, packaging, documentation, test, and history audits found no unique current consumer or behavior, so both files were removed to prevent divergence. Visual Studio IntelliSense derives settings from the selected CMake preset; other editors may use its generated `compile_commands.json`.

## Visual Studio workflow on Windows

For the standard Windows setup, install dependencies once in the selected classic vcpkg checkout and use one of these ways to make it discoverable:

- **Persistent user environment:** set `VCPKG_ROOT` in the Windows user environment, then restart Visual Studio or close and reopen the folder so the IDE process inherits it.
- **Ignored user preset:** create `CMakeUserPresets.json` with the environment-map example above, then select that inherited user preset in Visual Studio. A valid explicit external root has highest priority.
- **User-wide vcpkg integration:** run `<external-vcpkg-root>\vcpkg.exe integrate install` once from the intended external checkout, then reopen Visual Studio. Perastage reads `%LOCALAPPDATA%\vcpkg\vcpkg.path.txt` when no acceptable external environment root is available.

Select a canonical Perastage Windows Ninja preset rather than an IDE-generated configuration. Visual Studio 18 may inject its bundled `VC\vcpkg` as `VCPKG_ROOT`; the bootstrap deliberately ignores that value, selects the registered external checkout, and resets the configure-process environment before loading vcpkg. Manifest mode and manifest auto-install remain disabled.

Typical setup:

1. Set `VCPKG_ROOT` and install the required `x64-windows` dependencies in the selected classic vcpkg checkout once.
2. Run `setup_windows.ps1 -Configuration Debug -CleanBuild -SkipBuild` from the repository root to validate the toolchain and selected build directory.
3. Open the repository folder in Visual Studio.
4. Select `Local Machine`.
5. Select `Windows x64 Debug (Ninja)` or `Windows x64 Release (Ninja)`.
6. Select the matching Ninja build preset.

If Visual Studio shows stale configuration errors after changing presets, close Visual Studio and run `setup_windows.ps1 -Configuration Debug -CleanBuild -SkipBuild` or remove only the affected `build\win-x64-*-ninja` directory before configuring again.

## Command-line build

List available presets:

```powershell
cmake --list-presets
```

Configure a Windows Debug Ninja build from a Visual Studio Developer PowerShell after installing dependencies in the selected classic vcpkg checkout:

```powershell
.\setup_windows.ps1 -Configuration Debug -CleanBuild -SkipBuild
cmake --preset win-x64-debug-ninja
```

Build it:

```powershell
cmake --build --preset win-debug-build-ninja
```

Configure a Windows Release Ninja build:

```powershell
cmake --preset win-x64-release-ninja
```

Build it:

```powershell
cmake --build --preset win-release-build-ninja
```

## macOS presets

The macOS presets use `VCPKG_ROOT`, because the vcpkg installation path is usually developer-specific on macOS.

Make sure `VCPKG_ROOT` points to your macOS vcpkg installation before configuring:

```bash
export VCPKG_ROOT=/path/to/vcpkg
cmake --preset mac-arm64-debug
cmake --build --preset mac-debug-build
```

For a Release build:

```bash
export VCPKG_ROOT=/path/to/vcpkg
cmake --preset mac-arm64-release
cmake --build --preset mac-release-build
```

## Native Linux and WSL presets

The historically named WSL/Linux presets are the supported local path on both
native x64 Linux and x64 WSL. They use system packages, are enabled whenever
the CMake host is Linux, and intentionally ignore Windows vcpkg paths under
`/mnt/c` so WSL package discovery cannot cross into an incompatible Windows
dependency tree.

Use the WSL presets from a Linux/WSL environment where the required development packages are installed:

```bash
cmake --preset wsl-x64-debug
cmake --build --preset wsl-debug-build
```

For a Release build:

```bash
cmake --preset wsl-x64-release
cmake --build --preset wsl-release-build
```

## Quick Build without presets

A basic non-preset build can still be used when all dependencies are available through the system toolchain or a configured package manager:

```bash
cmake -S . -B build
cmake --build build --config Release
```

For regular development, prefer the project presets because they keep build directories, toolchains, and options consistent.

## Running Tests

Tests are normally enabled for Debug builds.

For a Windows Debug Ninja build:

```powershell
ctest --test-dir build/win-x64-debug-ninja --output-on-failure
```

For a WSL/Linux Debug build:

```bash
ctest --test-dir build/wsl-x64-debug --output-on-failure
```

## Build Targets and Outputs

- Application target: `Perastage` executable.
- `perastage_stage`: stages runtime files in `out/install/<CONFIG>` for packaging.
- `perastage_symbols`: collects symbol artifacts on supported Windows environments.

## Common configuration issue: dependency not found through vcpkg

If CMake reports that a required dependency cannot be found, for example:

```text
Could not find a package configuration file provided by "wxWidgets"
```

or:

```text
Could NOT find ZLIB (missing: ZLIB_LIBRARY ZLIB_INCLUDE_DIR)
```

or a similar error for `tinyxml2`, `CURL`, `GLEW`, `meshoptimizer`, `nanovg`, `podofo`, `Backward`, or `mdns`, the most common cause is that CMake is using a different vcpkg installation than the one where Perastage dependencies were installed.

First verify that the dependency is installed in the intended vcpkg instance. For example:

```powershell
& "$env:VCPKG_ROOT\vcpkg.exe" install --triplet x64-windows
```

Then verify that the expected vcpkg instance exists:

```powershell
Test-Path "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
Test-Path "$env:VCPKG_ROOT\installed\x64-windows\share\wxwidgets"
Test-Path "$env:VCPKG_ROOT\installed\x64-windows\include\zlib.h"
```

If the error path contains Visual Studio's internal vcpkg, for example:

```text
C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake
```

then Visual Studio exposed its bundled vcpkg instead of the intended external checkout. Select the canonical Windows Ninja preset, clear the affected CMake cache with `setup_windows.ps1 -CleanBuild -SkipBuild`, and verify that `CMAKE_TOOLCHAIN_FILE` points at `cmake/PerastageWindowsVcpkgToolchain.cmake` while `PERASTAGE_RESOLVED_VCPKG_ROOT` identifies the external checkout.

## Secure credential-store verification

Official Windows and macOS presets require `PERASTAGE_REQUIRE_SECURE_CREDENTIAL_STORE=ON`. Official Linux packaging workflows also pass this option explicitly. When the option is enabled, CMake compiles a small `wx/setup.h` probe and fails if `wxUSE_SECRETSTORE` is disabled. On Linux, building wxWidgets with this feature requires libsecret development headers such as `libsecret-1-dev`; runtime persistence still depends on a running Freedesktop Secret Service provider such as GNOME Keyring or KWallet.

For a manual Windows functional check after rebuilding dependencies:

1. Save valid GDTF Share credentials in Perastage.
2. Restart Perastage.
3. Open the GDTF download workflow.
4. Confirm no secure-storage persistence warning appears.
5. Confirm credentials are not requested again solely because the application restarted.
6. Confirm the test entry is present in Windows Credential Manager.

### Release-gate credential/security tests

Use a focused test build when validating GDTF Share credential storage for release:

```powershell
cmake -S . -B build-security -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DVCPKG_MANIFEST_MODE=OFF `
  -DVCPKG_MANIFEST_INSTALL=OFF `
  -DBUILD_TESTING=ON `
  -DPERASTAGE_REQUIRE_SECURE_CREDENTIAL_STORE=ON
cmake --build build-security --target gdtf_share_security_test credential_store_native_roundtrip_test
ctest --test-dir build-security -L release-gate --output-on-failure
```

The native credential-store round-trip test uses a unique `Perastage/Test/SecureStore/...` service name, never uses the production `Perastage/GDTF Share/gdtf-share.com` entry, and may report a CTest skip when the runner does not expose Windows Credential Manager, macOS Keychain, or a Linux Secret Service provider. A compile-time secure-store probe is required for official builds, but it does not replace a manual save, restart, download, and clear validation on a real Windows desktop.

Manual Windows release validation:

1. Start from a build configured with `wxUSE_SECRETSTORE` enabled.
2. Open Preferences -> GDTF Share credentials.
3. Enter and validate valid credentials.
4. Confirm no secure-storage warning appears.
5. Close Perastage completely.
6. Reopen Perastage.
7. Open GDTF download.
8. Confirm the online catalog loads.
9. Select and download one GDTF.
10. Confirm credentials are not requested again.
11. Confirm a Perastage entry exists in Windows Credential Manager.
12. Clear credentials from Perastage.
13. Confirm the native entry is removed.
14. Repeat with a password containing a double quote, a backslash, and Unicode text.

### Windows Ninja x64 compiler validation

Windows Ninja presets whose names contain `win-x64` request an external x64 Visual Studio environment. Visual Studio uses that preset metadata to source x64 tools before invoking CMake, and `setup_windows.ps1` separately verifies that `cl.exe`, `link.exe`, `VSCMD_ARG_HOST_ARCH`, `VSCMD_ARG_TGT_ARCH`, and the compiler banner all identify an x64 toolchain before configuring.

A `LNK4272` message saying x64 libraries conflict with an x86 target, especially during `CMakeTestCXXCompiler.cmake`, means an old build directory cached an x86 compiler or a compiler from a different Visual Studio installation while the current environment points at x64 libraries. The setup script checks `CMakeCache.txt` before configure and removes only the selected build directory when it finds an incompatible x86 compiler path, different Visual Studio root, different generator, different toolchain, or different vcpkg triplet.

Use `-CleanBuild` to force the same safe cleanup for the selected build directory:

```powershell
.\setup_windows.ps1 -Configuration Debug -CleanBuild -SkipBuild
```

Use `-VisualStudioPath` or `-VisualStudioVersion` to make multi-install selection explicit. The cleanup does not delete source files, the selected classic vcpkg checkout, `$env:VCPKG_ROOT\installed`, global vcpkg downloads, packages, buildtrees, or unrelated build directories.

## CI vcpkg retry, cache, and diagnostics policy

CI keeps `vcpkg.json` as the dependency source of truth and reads the pinned 40-character `builtin-baseline` before cloning vcpkg. Dependency installation stays in classic mode: workflows pass explicit install, packages, downloads, and binary-cache roots under `${{ github.workspace }}/.vcpkg-cache` and continue to configure CMake with `VCPKG_MANIFEST_MODE=OFF`.

The vcpkg cache schema is currently `v3`. Change that schema string in the workflow cache keys to intentionally invalidate all local GitHub Actions vcpkg caches without changing dependency versions. Compiled package keys follow this documented shape:

```text
vcpkg-compiled-v3-<runner.os>-<runner.arch>-<triplet>-<toolchain-or-sdk-scope>-<baseline>-<hashFiles('vcpkg.json', 'vcpkg-configuration.json')>
```

Downloads use the matching `vcpkg-downloads-v3-...` prefix and remain separate from compiled packages. The manifest hash covers `vcpkg.json` and the optional `vcpkg-configuration.json` input when present. Workflow filenames, job names, Debug/Release labels, CTest settings, package staging, and unrelated source files are intentionally excluded so ordinary workflow edits do not rebuild stable dependencies.

The compiled cache contains only `.vcpkg-cache/installed`, `.vcpkg-cache/packages`, and `.vcpkg-cache/binary`. The downloads cache contains only `.vcpkg-cache/downloads`. Build trees, object files, CTest results, staged installers, and final artifacts are not cached by this policy. PAT-backed caches, third-party feeds, sccache, and ccache remain intentionally out of scope; the repository-owned GitHub Packages layer is described below.

Each affected workflow restores downloads and compiled vcpkg caches before bootstrapping and installing vcpkg. After `vcpkg_install_retry.py` succeeds, workflows explicitly save downloads and then save the compiled cache with `actions/cache/save@v5` using each restore step's `cache-primary-key`. This save is placed before CMake configure, project compilation, CTest, packaging, artifact staging, and installer validation so a later failure cannot discard successfully built dependencies. The compiled cache save is skipped when the restore step reported an exact hit, preventing duplicate saves.

Compatible workflows share cache keys when their ABI inputs match. Windows Debug CI and the Windows Release installer share `x64-windows` caches. Linux Debug CI and the Linux Release installer share `x64-linux` caches. The Arch package workflow keeps an `arch` toolchain scope because its container toolchain and system packages differ from Ubuntu. macOS cache keys keep an SDK/Xcode boundary: macOS 26 installer jobs use their fixed Xcode 26 scope, macOS 15 manual installer jobs include the deployment target, and macOS Debug CI includes a hash of the active Xcode and SDK real path.

Every vcpkg job writes a GitHub Step Summary with the platform, runner architecture, triplet, baseline, schema, cache hits, primary key, and explicit-save outcome. To diagnose a cache miss, compare those summary fields across runs. A changed schema, runner OS or architecture, triplet, macOS SDK/Xcode identity, vcpkg baseline, `vcpkg.json`, or supported `vcpkg-configuration.json` hash should create a new primary key. If none of those inputs changed, inspect the restore logs for eviction or branch-scope cache availability.

All GitHub Actions installer workflows run vcpkg through `.github/scripts/vcpkg_install_retry.py` instead of invoking `vcpkg install` directly. The helper retries only failures that match transient download or network signatures, such as HTTP 408, 425, 429, 500, 502, 503, or 504 responses, DNS failures, timeouts, connection resets, refused connections, and temporary proxy or remote-server failures. Configure, compile, link, manifest, ABI, patch, and package-validation failures are treated as permanent and fail without hiding the original vcpkg exit code.

The retry helper always writes a complete UTF-8 log under `out/ci-logs/` while streaming vcpkg output live to the Actions log. Failure diagnostics upload `out/ci-logs/**`, vcpkg buildtree logs, vcpkg issue bodies, and CMake logs when CMake was reached. Missing CMake logs after an earlier vcpkg download failure are expected and should not be treated as a separate diagnostic problem. A prolonged upstream outage, such as repeated HTTP 504 responses from a dependency host, can still exhaust the bounded retries; in that case maintainers should rerun the failed package job later rather than changing the vcpkg baseline, vendoring the dependency, or adding an unverified mirror.

### Two-level vcpkg binary cache

GitHub Actions now uses two ordered dependency-cache layers. The existing `actions/cache` layer remains first and stores downloads separately from the installed tree, packages tree, and local file-based binary archives under `.vcpkg-cache`. GitHub Packages is the persistent second layer and stores vcpkg binary packages in the `PeramatoG` NuGet feed at `https://nuget.pkg.github.com/PeramatoG/index.json`; packages carry repository metadata for `https://github.com/PeramatoG/Perastage`. CMake build directories, test results, and compiler outputs are not cached.

CI Debug and the Windows, Ubuntu, current macOS, and macOS 15 installer workflows request the remote provider in `read` mode. Their explicit `packages: read` permission, and the same permission on reusable-workflow callers, is required because a called workflow cannot elevate its caller's token. Pull requests and installers never publish. If a read token, source registration, Mono, or the service is unavailable during setup, the helper warns and retains the local file provider so caching remains an optional optimization. Non-Windows jobs execute vcpkg's fetched `nuget.exe` through Mono; Ubuntu installs Mono explicitly and macOS verifies or installs it.

Only **vcpkg Binary Cache** may use `readwrite`. It runs trusted `main` code with `contents: read` and `packages: write`, warms `x64-windows`, Ubuntu `x64-linux`, and current-runner `arm64-osx`, and fails if authentication, setup, install, or upload fails. A preflight rejects every ref except `refs/heads/main`, and the warmer checks out the exact guarded event commit. The temporary NuGet configuration defines the PeramatoG feed as its default push source. Run the workflow from Actions with **Run workflow** after changing `vcpkg.json` or its pinned builtin baseline. No PAT or additional repository secret is needed.

Validate and initially seed the remote cache with two manual runs on `main`, both with `bypass_local_compiled_cache=true`. On the first run, the workflow skips local compiled-cache restore and save so already-installed local packages cannot hide missing remote packages; downloads caching remains enabled, and missing binaries are built and published. Inspect each matrix summary and install log, then confirm the resulting NuGet packages are associated with Perastage in the organization/repository Packages settings. On the unchanged second run with the same bypass enabled, confirm heavy dependencies restore from GitHub Packages without rebuilding and compare install duration. No existing cache is deleted or invalidated. vcpkg does not expose stable structured per-provider package counts at the pinned baseline, so the summary separately reports provider configuration and the vcpkg install outcome, and explicitly says that publication is not independently verified. The retained install log and the Packages page supply the non-secret package-level evidence; job success alone is not described as publication proof.

Package-writing runs share a non-cancelling workflow concurrency group, so a manual validation waits for an active path-filtered run instead of publishing the same package versions concurrently. The pinned vcpkg tool performs NuGet publication itself and does not pass NuGet's `-SkipDuplicate` option. The repository therefore does not suppress HTTP 409 conflicts or any other publication failure. After serialization, an unchanged isolated validation run must restore already-published packages rather than rebuild and republish them. If an isolated run still returns 409, retain its credential-safe diagnostics and design a separate publication change from that evidence; do not patch the setup helper, broadly ignore push errors, or replace the pinned tool as a workaround.

The warming workflow resolves the current Xcode version and canonical macOS SDK path into the same SDK-aware cache identity used by macOS Debug CI. It exports both the selected and resolved SDK paths and runs `macos_sdk_cache_guard.py` before installation, preventing restored local packages containing stale SDK metadata from crossing Xcode/SDK compatibility boundaries. Windows and Ubuntu retain their `default` cache scope.

For subsequent remote-layer checks, use the same `bypass_local_compiled_cache` mode. It skips only compiled-cache restore/save for that run while retaining the same baseline, manifest, triplet, SDK, and ABI inputs. Confirm that unchanged packages, especially wxWidgets, restore rather than compile. Downloads caching remains enabled.

For access failures, check the summary's setup result and local-only fallback, the workflow's effective `packages` permission, Mono availability, package visibility, repository Actions access, inherited permissions, and package association. Never upload or print the temporary NuGet configuration. After dependency or baseline changes, let the path-filtered warmer publish new ABI packages; old production caches need not be deleted.

Arch packaging deliberately remains local-cache-only because its rolling toolchain and ABI cannot safely consume Ubuntu `x64-linux` packages. Remote Arch caching requires a dedicated trusted Arch warming environment.

## CI Debug compiler caching

CI Debug Tests uses Mozilla sccache only for Perastage and test C/C++ compilation. This is separate from vcpkg binary caching: vcpkg restores dependency packages before sccache starts, while sccache reuses individual project object compilations. Installer, release, Arch, and vcpkg-warming workflows remain outside this PR 3A scope; installer evaluation is reserved for PR 3B.

The workflow pins `mozilla-actions/sccache-action` v0.0.10 at commit `9e7fa8a12102821edf02ca5dbea1acd0f89a2696` and installs sccache v0.15.0. It uses the native GitHub Actions backend, an absolute `SCCACHE_BASEDIRS` workspace path, and schema `perastage-ci-debug-v1`. Compatibility suffixes include runner OS and architecture, compiler/toolset identity, Debug, and the existing Xcode/SDK identity on macOS. Increment `v1` deliberately when incompatible cache semantics require a clean namespace; do not add a commit, branch, pull-request, or run identifier.

Pull requests use the native GHA backend in GitHub's `refs/pull/<number>/merge` cache scope. Objects written there can be restored by reruns of that pull request, but not by `main` or sibling pull requests; a pull request may also restore compatible default-branch objects. A push to `main` automatically warms `gha-main` only when the checked-out SHA equals freshly fetched `origin/main` and no source override exists. The warmer restores dependencies, validates and configures the unchanged Debug test build, builds `--target all`, and records compiler-cache diagnostics, while skipping only CTest execution and test-result upload. A manual dispatch uses `gha-main` only when the workflow runs from `refs/heads/main`, the requested source is empty or explicitly `main`, and its exact SHA equals freshly fetched `origin/main`; it remains a normal complete test run. Reusable workflow calls, non-main pushes, mismatched SHAs, and arbitrary manual sources disable GHA and use an unpersisted local directory for within-job reuse. GitHub's runtime credentials are supplied by the official action and must never be printed or uploaded; no PAT or additional secret is used. `SCCACHE_GHA_RW_MODE` is intentionally absent because sccache v0.15.0 does not support it.

Linux records the actual GCC version and target. Windows records the initialized Hostx64/x64 MSVC toolset and Windows SDK, sets CMP0141 to `NEW` for this configure, and requests CMake's `Embedded` debug-information format. The generated compile database must contain exact `Z7` flags with either valid MSVC prefix (`-Z7` or `/Z7`) and no `Zi` or `ZI` equivalent; linker PDB creation and Debug runtime behavior are otherwise unchanged. macOS reuses the workflow's established Xcode/SDK hash and records the active Apple Clang identity.

Each configure loads a generated CMake initial-cache file that safely bracket-quotes exact executable paths. On Windows, the official action exports an intentionally extensionless `SCCACHE_PATH`; PowerShell resolves the installed `sccache.exe` with `Get-Command`, verifies that exact leaf path and suffix, and stores it separately as `PERASTAGE_SCCACHE_EXECUTABLE`. The Windows initial cache also transports the exact Hostx64/x64 `cl.exe`, Git Bash, CMP0141 policy default, and embedded debug-information format, keeping every executable path off the native PowerShell-to-Python-to-CMake argument chain. Configure, exact toolchain/launcher validation, and structural compile-flag validation are separate steps with separate diagnostic logs. The flag validator classifies explicit C/C++, Windows resource, and unexpected entries; it requires exact `-Z7` or `/Z7` only for C/C++, rejects exact `-Zi`, `/Zi`, `-ZI`, and `/ZI`, and verifies that `.rc` entries use `rc.exe` without applying C/C++ debug flags. Unknown extensions and compiler/language mismatches fail instead of being skipped. Each job zeros job-local counters without deleting remote objects and captures text and v0.15.0 JSON statistics under `out/ci-logs/sccache-<platform>-debug.*`. The Step Summary reports the configured backend and cache scope, namespace, compiler, normalized base directory, requests, executed requests, compilations, failures, hits, misses, writes, read/write errors, non-cacheable work, and hit rate. A successful build with zero requests or an unvalidated launcher fails. When configure or build already failed, the always-run statistics step reports that state without creating a second launcher failure. `SCCACHE_IGNORE_SERVER_IO_ERROR=1` lets transient backend I/O fall through to the real compiler, but installation, launcher, compiler, build, malformed-statistics, and test failures remain fatal. CTest runs normally for pull requests, manual dispatches, and reusable calls; only the trusted automatic `main` warmer omits it, and CTest duration is never cached.

The latest PR run on 2026-07-25 proved native object reuse on Linux with 1,814 requests, 1,813 hits, one miss, one write, and no read/write errors, and on macOS with 15 requests, 15 hits, no misses, and no writes or errors. The subsequent unchanged Linux CTest command ran the complete suite and exposed unrelated existing failures; compiler caching must not skip or weaken those tests. Windows still requires a follow-up real run to prove compilation through the newly resolved `sccache.exe` path.

Low hit rates are expected after source, flags, compiler, toolset, SDK, or schema changes; compare the recorded compatibility fields and cache errors before changing the namespace. Zero requests indicate that CMake did not use the launcher and must not be treated as a cold-cache result.

### Post-merge two-run validation

1. In PR CI, confirm all three platforms report v0.15.0, the expected real compiler and launcher, `gha-pr-merge-ref`, and more than zero requests. Record hits, misses, writes, and errors. Builds, CTest selection, and vcpkg read mode must remain unchanged; zero hits are acceptable on a cold scope.
2. After merge, open **Actions → CI Debug Tests → Run workflow**, select workflow branch `main`, leave `source_ref` empty (or use `main`), select profile `pr`, and start a new run. Confirm the resolved SHA is current `main` and the scope is `gha-main`. Wait for the complete run. For every platform record total job, vcpkg install, configure, C/C++ build, and CTest durations plus requests, hits, misses, writes, non-cacheable compilations, and read/write errors.
3. Start a new workflow run with exactly the same settings after the first completes; do not use only **Re-run jobs** and do not change source, workflow, toolchain, profile, or SDK inputs. Confirm fresh build directories still produce requests, hits become substantially greater than zero, misses decrease, the build step improves meaningfully, CTest still executes independently, and vcpkg restores through its existing layers.
4. Proceed to PR 3B only if Linux, Windows, and macOS prove launcher use and real second-run hits, toolchain identities and Windows symbols remain correct, tests are unchanged, build savings are meaningful, errors are absent or understood, and no PR can write into a trusted cache boundary. If a platform does not benefit, keep it disabled and document the evidence.
