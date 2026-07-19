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

Perastage uses the root `vcpkg.json` manifest as the dependency source of truth. The manifest pins the vcpkg builtin baseline and requests both `wxwidgets[secretstore]` and the Windows-only host dependency `gettext[tools]`, so Windows localization tools and secure-store-enabled wxWidgets are installed by one manifest install.

The recommended Windows setup is:

```powershell
cd C:\path\to\Perastage
.\setup_windows.ps1 -Configuration Debug -CleanBuild
```

By default, `setup_windows.ps1` creates or reuses the Perastage-specific checkout at `.tools\vcpkg`, checks out the pinned baseline in detached HEAD mode, bootstraps vcpkg after checkout, installs the manifest into the repository-local `vcpkg_installed` tree, and passes that same path to CMake as `VCPKG_INSTALLED_DIR`. Use `-VcpkgRoot C:\path\to\dedicated\vcpkg` only when you intentionally want another checkout. The script refuses to change a dirty vcpkg checkout; clean or stash that checkout or use the default Perastage-local checkout.

If an older build was configured against wxWidgets without `secretstore` or another installed root, rerun the script with `-CleanBuild` to delete only the selected Perastage build directory before reconfiguring. Do not run a separate package-argument gettext install from the repository root.

Gettext tools are build-time dependencies for localization catalog generation. They are not Perastage runtime dependencies. Homebrew gettext is keg-only on macOS; add `$(brew --prefix gettext)/bin` to `PATH` before configuring CMake so `msgfmt`, `xgettext`, `msgmerge`, and `msgattrib` resolve consistently.

The shared Windows presets keep `C:/vcpkg` as a stable fallback and leave vcpkg manifest mode enabled so opening the project directly in Visual Studio can install missing manifest dependencies into the normal `C:/vcpkg/installed` tree. `setup_windows.ps1` writes an ignored `CMakeUserPresets.json` for the selected checkout and `vcpkg_installed` tree; those generated local presets set `VCPKG_MANIFEST_MODE=OFF` because setup already performed the explicit manifest install. If you intentionally use another vcpkg checkout, pass `-VcpkgRoot` to `setup_windows.ps1` or maintain a local `CMakeUserPresets.json` that overrides both `CMAKE_TOOLCHAIN_FILE` and `VCPKG_INSTALLED_DIR`.

`CMakeUserPresets.json` is local to each developer machine and should not be committed. Run Ninja presets from a Visual Studio Developer PowerShell or through `setup_windows.ps1`; otherwise CMake may report `CMAKE_CXX_COMPILER not set` because `cl.exe` is not initialized.

## CMake presets strategy

Perastage uses CMake presets for repeatable local and CI builds.

```text
CMakePresets.json
CMakeUserPresets.json
```

`CMakePresets.json` is the shared project-level preset file and is tracked in Git.

`CMakeUserPresets.json` is local to each developer machine. Use it only when local toolchain paths or environment overrides are needed.

## Optional local Windows user presets

If you intentionally use a vcpkg checkout outside `.tools/vcpkg`, create a local file named:

```text
CMakeUserPresets.json
```

in the repository root.

This file should not be committed. It should contain local machine settings only.

Example for a Windows machine using vcpkg in `D:/tools/vcpkg`:

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "local-win-x64-debug-ninja",
      "displayName": "Local Windows x64 Debug (Ninja)",
      "description": "Local Debug Ninja build using a custom vcpkg installation.",
      "inherits": "win-x64-debug-ninja",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "D:/tools/vcpkg/scripts/buildsystems/vcpkg.cmake",
        "VCPKG_INSTALLED_DIR": "${sourceDir}/vcpkg_installed",
        "VCPKG_MANIFEST_MODE": "OFF"
      }
    },
    {
      "name": "local-win-x64-release-ninja",
      "displayName": "Local Windows x64 Release (Ninja)",
      "description": "Local Release Ninja build using a custom vcpkg installation.",
      "inherits": "win-x64-release-ninja",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "D:/tools/vcpkg/scripts/buildsystems/vcpkg.cmake",
        "VCPKG_INSTALLED_DIR": "${sourceDir}/vcpkg_installed",
        "VCPKG_MANIFEST_MODE": "OFF"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "local-win-debug-build-ninja",
      "displayName": "Local Build Windows Debug (Ninja)",
      "description": "Build the local Windows Debug Ninja configuration.",
      "configurePreset": "local-win-x64-debug-ninja",
      "jobs": 8
    },
    {
      "name": "local-win-release-build-ninja",
      "displayName": "Local Build Windows Release (Ninja)",
      "description": "Build the local Windows Release Ninja configuration.",
      "configurePreset": "local-win-x64-release-ninja",
      "jobs": 8
    }
  ]
}
```

For the recommended setup-script workflow, this file is generated automatically when it is missing.

## Visual Studio workflow on Windows

For the standard Windows setup, run `setup_windows.ps1` first, then use the generated local Windows presets in Visual Studio. The generated local presets deliberately set `VCPKG_MANIFEST_MODE=OFF` because the setup script already ran the one manifest install into `vcpkg_installed`; this prevents CMake configure from launching a second vcpkg install into another tree. If you use only the shared presets without running setup, CMake/vcpkg manifest mode remains enabled and uses `C:/vcpkg/installed` so dependencies such as wxWidgets can be installed automatically without depending on the setup-script `vcpkg_installed` tree.

Typical setup:

1. Run `setup_windows.ps1 -Configuration Debug -CleanBuild` from the repository root.
2. Open the repository folder in Visual Studio.
3. Select `Local Machine`.
4. Select a Windows configure preset such as `Windows x64 Debug (Ninja)`.
5. Select a Windows build preset such as `Build Windows Debug (Ninja)`.

If Visual Studio shows stale configuration errors after changing presets, close Visual Studio and remove the local `.vs` folder and the affected build directory before configuring again.

## Command-line build

List available presets:

```powershell
cmake --list-presets
```

Configure a Windows Debug Ninja build from a Visual Studio Developer PowerShell after running the setup script:

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

then Visual Studio is using its own vcpkg instance instead of the expected setup-managed checkout or explicit `C:/vcpkg` fallback. Run `setup_windows.ps1`, make sure `CMakeUserPresets.json` points at the selected checkout, clear the CMake cache, and remove stale `.vs` or build folders if needed.

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
  -DCMAKE_TOOLCHAIN_FILE=".tools\vcpkg\scripts\buildsystems\vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DVCPKG_INSTALLED_DIR="$PWD\vcpkg_installed" `
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

Use `-VisualStudioPath` or `-VisualStudioVersion` to make multi-install selection explicit. The cleanup does not delete source files, `.tools\vcpkg`, `vcpkg_installed`, global vcpkg downloads, or shared vcpkg checkouts.
