# Build and Dependency Guide

This document covers baseline and advanced build behavior for Perastage. It is the detailed companion to the short installation section in `README.md`.

Repository path and CMake ownership are defined by
[Repository Layout](repository_layout.md) and [Architecture](architecture.md);
this guide owns build prerequisites and procedures.

## Core Requirements

- CMake 3.21 or newer.
- C++20-capable compiler/toolchain.
- wxWidgets 3.3.1 or compatible development package.
- Required libraries:
  - wxWidgets
  - tinyxml2
  - libxml2 with XSD support
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
& "$env:VCPKG_ROOT\vcpkg.exe" install --triplet x64-windows wxwidgets[secretstore] gettext[tools] tinyxml2 libxml2[core] curl glew zlib nanovg podofo meshoptimizer backward-cpp mdns
```

Gettext tools are build-time dependencies for localization catalog generation. On Windows they should resolve from `$env:VCPKG_ROOT\installed\x64-windows\tools\gettext\bin`. They are not Perastage runtime dependencies. Homebrew gettext is keg-only on macOS; add `$(brew --prefix gettext)/bin` to `PATH` before configuring CMake so `msgfmt`, `xgettext`, `msgmerge`, and `msgattrib` resolve consistently.

The complete Windows Debug CTest workflow also requires
[ripgrep](https://github.com/BurntSushi/ripgrep) (`rg`) on `PATH`, because the
registered shell policy tests use it for repository inspection. Ripgrep is a
development/test tool, not an application runtime dependency. The Debug setup
preflight reports an actionable error before configure when it is unavailable;
a Release-only application build does not require it.

Use the setup script as a validator/build helper, not as an installer:

```powershell
cd C:\path\to\Perastage
$env:VCPKG_ROOT = 'D:\path\to\vcpkg'
.\setup_windows.ps1 -Configuration Debug -CleanBuild -SkipBuild
# Optional explicit Git Bash override:
.\setup_windows.ps1 -Configuration Debug -CleanBuild -SkipBuild -BashExecutable "C:\Program Files\Git\bin\bash.exe"
```

The root `setup_windows.ps1` command is the stable public entry point and may
be invoked from any working directory by path. It delegates implementation to
`scripts/windows/`, but developers should continue to use the root command.

`setup_windows.ps1` resolves the checkout from explicit `-VcpkgRoot` first, a valid external `VCPKG_ROOT` second, and the standard user-wide integration descriptor third; it fails if none identifies a valid external checkout. It validates `vcpkg.exe`, `.vcpkg-root`, `scripts\buildsystems\vcpkg.cmake`, `installed\x64-windows`, representative package headers and metadata (including libxml2), gettext tools, and `wxUSE_SECRETSTORE` in the generated Debug and Release MSW configuration headers under the vcpkg library directories. The generic public `include\wx\setup.h` is not a generated platform configuration and is not used for this feature check. Before invoking the shared preset it exports the resolved root as `VCPKG_ROOT`, so validation and CMake cannot select different installations. It also imports and validates an x64 MSVC environment and removes only the selected Perastage build directory when a stale incompatible CMake cache is detected. It does not clone vcpkg, bootstrap vcpkg, run vcpkg installs, generate `CMakeUserPresets.json`, create `.tools\vcpkg`, or create a repository-local `vcpkg_installed` tree.

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

The root `setup.sh` launcher installs the distro packages needed by dependency
discovery, `ripgrep` for repository policy tests, and the Spanish and Simplified
Chinese locales used by the complete CTest suite on its supported apt path.
Ripgrep is a development/test dependency, not an application runtime
dependency. The MVR-xchange `mdns` package
is not available from those distro package sets. Supply it externally as
documented in [MVR-xchange Notes](technical-notes/mvr_xchange.md), and expose
the prefix containing `mdnsConfig.cmake` through `CMAKE_PREFIX_PATH` before
using the canonical preset. For example, for an external vcpkg installed tree:

```bash
export CMAKE_PREFIX_PATH=/path/to/vcpkg-installed/x64-linux
```

With dependency installation enabled, the launcher runs the supported package
manager before validating that CMake is available. With `--skip-deps`, it
performs no package installation and requires CMake to be preinstalled.

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

## Hosted CI behavior

Hosted dependency caching, retry behavior, compiler caching, diagnostics, and
workflow orchestration are owned by the
[GitHub Actions workflow architecture](github_actions_workflows.md). This build
guide intentionally covers developer-local setup, configure, build, and test
workflows only.
