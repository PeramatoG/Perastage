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

Perastage keeps the root `vcpkg.json` manifest as the dependency source of truth for CI and for documenting the required packages. The normal local Windows workflow is intentionally classic vcpkg: install dependencies once into `C:\vcpkg\installed\x64-windows`, then configure with the shared Ninja presets. Visual Studio and CMake must not run an automatic vcpkg install during local configure.

The canonical local Windows presets are:

- `win-x64-debug-ninja`
- `win-x64-release-ninja`

Both presets use `C:/vcpkg/scripts/buildsystems/vcpkg.cmake`, `VCPKG_TARGET_TRIPLET=x64-windows`, `VCPKG_MANIFEST_MODE=OFF`, and `VCPKG_MANIFEST_INSTALL=OFF`. This makes CMake resolve already-installed packages from `C:/vcpkg/installed/x64-windows` and prevents `-- Running vcpkg install` during a clean Visual Studio configure.

Install or repair dependencies manually before configuring if they are missing. A typical one-time command is:

```powershell
C:\vcpkg\vcpkg.exe install --triplet x64-windows wxwidgets[secretstore] gettext[tools] tinyxml2 curl glew zlib nanovg podofo meshoptimizer backward-cpp mdns
```

Gettext tools are build-time dependencies for localization catalog generation. On Windows they should resolve from `C:\vcpkg\installed\x64-windows\tools\gettext\bin`. They are not Perastage runtime dependencies. Homebrew gettext is keg-only on macOS; add `$(brew --prefix gettext)/bin` to `PATH` before configuring CMake so `msgfmt`, `xgettext`, `msgmerge`, and `msgattrib` resolve consistently.

Use the setup script as a validator/build helper, not as an installer:

```powershell
cd C:\path\to\Perastage
.\setup_windows.ps1 -Configuration Debug -CleanBuild -SkipBuild
# Optional explicit Git Bash override:
.\setup_windows.ps1 -Configuration Debug -CleanBuild -SkipBuild -BashExecutable "C:\Program Files\Git\bin\bash.exe"
```

By default, `setup_windows.ps1` validates `C:\vcpkg`, `vcpkg.exe`, `scripts\buildsystems\vcpkg.cmake`, `installed\x64-windows`, representative package headers, gettext tools, and `wxUSE_SECRETSTORE`. It also imports and validates an x64 MSVC environment and removes only the selected Perastage build directory when a stale incompatible CMake cache is detected. It does not clone vcpkg, bootstrap vcpkg, run vcpkg installs, generate `CMakeUserPresets.json`, create `.tools\vcpkg`, or create a repository-local `vcpkg_installed` tree.

If an older build was configured against wxWidgets without `secretstore`, manifest mode, another installed root, or an x86 compiler, rerun the script with `-CleanBuild` to delete only the selected Perastage build directory before reconfiguring. Deleting `.vs` or `build` does not require reinstalling packages, and deleting `C:\vcpkg\installed` is not part of normal troubleshooting.


### Canonical local Windows x64 bootstrap

`setup_windows.ps1` is the canonical local Windows x64 Ninja entry point. A generic Visual Studio Developer PowerShell can expose Hostx86/x86 tools depending on how it was launched, so the script always imports `VsDevCmd.bat -host_arch=x64 -arch=x64` itself and verifies that `cl.exe`, `link.exe`, `VSCMD_ARG_HOST_ARCH`, and `VSCMD_ARG_TGT_ARCH` all describe Hostx64/x64 before CMake configure starts.

Git Bash does not need to be first on `PATH`. The setup script accepts an optional explicit override through `-BashExecutable` or the `BASH_EXECUTABLE` environment variable; a valid explicit Git Bash wins over automatic discovery. Without an override, the script resolves Git for Windows, derives `bash.exe` from that installation, rejects WSL/System32 and WindowsApps launchers, runs a non-login shell probe, and passes the resolved path to CMake as `-DBASH_EXECUTABLE=...`. If Git for Windows is missing or only a launcher is available, the expected failure is:

```text
Git Bash could not be resolved. Install Git for Windows or pass -DBASH_EXECUTABLE=<Git for Windows bash.exe>; WSL and WindowsApps bash launchers are not supported.
```

Use this clean Debug validation command after installing Visual Studio C++ tools, Git for Windows, Ninja, CMake, and classic `C:\vcpkg` dependencies:

```powershell
.\setup_windows.ps1 -Configuration Debug -CleanBuild -SkipBuild
```

After a successful configure, inspect the cache entries that prove the intended tools were selected:

```powershell
Select-String -Path build\win-x64-debug-ninja\CMakeCache.txt -Pattern '^(BASH_EXECUTABLE|CMAKE_C_COMPILER|CMAKE_CXX_COMPILER|VCPKG_TARGET_TRIPLET):'
```

Expected values are a Git-for-Windows `bash.exe`, MSVC compilers under `VC\Tools\MSVC\...\bin\Hostx64\x64`, and `VCPKG_TARGET_TRIPLET:STRING=x64-windows`.

## CMake presets strategy

Perastage uses CMake presets for repeatable local and CI builds.

```text
CMakePresets.json
CMakeUserPresets.json
```

`CMakePresets.json` is the shared project-level preset file and is tracked in Git. Windows local builds expose one x64 Ninja path with Debug and Release configure presets plus matching build/stage presets.

`CMakeUserPresets.json` is local to each developer machine and should not be committed. Perastage no longer generates this file. Use it only if you intentionally need local machine overrides, and do not use it to create another vcpkg checkout or repository-local installed tree for the normal Windows workflow.

## Visual Studio workflow on Windows

For the standard Windows setup, install dependencies once in `C:\vcpkg`, run `setup_windows.ps1` to validate the environment if desired, then open the repository folder in Visual Studio and select one of the canonical Ninja presets. Because manifest mode and manifest auto-install are disabled in the shared Windows presets, Visual Studio/CMake reuses `C:\vcpkg\installed\x64-windows` and should not print `-- Running vcpkg install`.

Typical setup:

1. Install the required `x64-windows` dependencies in `C:\vcpkg` once.
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

Configure a Windows Debug Ninja build from a Visual Studio Developer PowerShell after installing dependencies in `C:\vcpkg`:

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

## WSL/Linux presets

The WSL/Linux presets use system packages and intentionally ignore Windows vcpkg paths under `/mnt/c`.

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
C:\vcpkg\vcpkg.exe install --triplet x64-windows
```

Then verify that the expected vcpkg instance exists:

```powershell
Test-Path "C:\vcpkg\scripts\buildsystems\vcpkg.cmake"
Test-Path "C:\vcpkg\installed\x64-windows\share\wxwidgets"
Test-Path "C:\vcpkg\installed\x64-windows\include\zlib.h"
```

If the error path contains Visual Studio's internal vcpkg, for example:

```text
C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake
```

then Visual Studio is using its own vcpkg instance instead of the required `C:/vcpkg` toolchain. Select the canonical Windows Ninja preset, clear the affected CMake cache with `setup_windows.ps1 -CleanBuild -SkipBuild`, and verify that `CMAKE_TOOLCHAIN_FILE` still points at `C:/vcpkg/scripts/buildsystems/vcpkg.cmake`.

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
  -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" `
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

Use `-VisualStudioPath` or `-VisualStudioVersion` to make multi-install selection explicit. The cleanup does not delete source files, `C:\vcpkg`, `C:\vcpkg\installed`, global vcpkg downloads, packages, buildtrees, or unrelated build directories.

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

Only **vcpkg Binary Cache** may use `readwrite`. It runs trusted `main` code with `contents: read` and `packages: write`, warms `x64-windows`, Ubuntu `x64-linux`, and current-runner `arm64-osx`, and fails if authentication, setup, install, or upload fails. The temporary NuGet configuration defines the PeramatoG feed as its default push source. Run the workflow from Actions with **Run workflow** after changing `vcpkg.json` or its pinned builtin baseline. No PAT or additional repository secret is needed.

Validate the cache with two runs. On the first run, inspect each matrix summary and install log for compilation and authenticated read/write completion, then confirm the resulting NuGet packages are associated with Perastage in the organization/repository Packages settings. On the unchanged second run, confirm heavy dependencies are restored or already present, are not rebuilt, and compare install duration. vcpkg does not expose stable structured per-provider package counts at the pinned baseline, so the summary separately reports provider configuration and the vcpkg install outcome, and explicitly says that publication is not independently verified. The retained install log and the Packages page supply the non-secret package-level evidence; job success alone is not described as publication proof.

The warming workflow resolves the current Xcode version and canonical macOS SDK path into the same SDK-aware cache identity used by macOS Debug CI. It exports both the selected and resolved SDK paths and runs `macos_sdk_cache_guard.py` before installation, preventing restored local packages containing stale SDK metadata from crossing Xcode/SDK compatibility boundaries. Windows and Ubuntu retain their `default` cache scope.

To exercise GitHub Packages without deleting normal caches, manually run the warmer with `bypass_local_compiled_cache` enabled. This skips only compiled-cache restore/save for that run while retaining the same baseline, manifest, triplet, SDK, and ABI inputs. Confirm that unchanged packages, especially wxWidgets, restore rather than compile. Downloads caching remains enabled.

For access failures, check the summary's setup result and local-only fallback, the workflow's effective `packages` permission, Mono availability, package visibility, repository Actions access, inherited permissions, and package association. Never upload or print the temporary NuGet configuration. After dependency or baseline changes, let the path-filtered warmer publish new ABI packages; old production caches need not be deleted.

Arch packaging deliberately remains local-cache-only because its rolling toolchain and ABI cannot safely consume Ubuntu `x64-linux` packages. Remote Arch caching requires a dedicated trusted Arch warming environment. Compiler caching with sccache remains future work.
