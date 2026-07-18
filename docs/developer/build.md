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

Perastage uses the repository `vcpkg.json` manifest for common C++ dependencies. On Windows, the recommended local convention is to install vcpkg in:

```text
C:/vcpkg
```

The shared Windows presets in `CMakePresets.json` use this default path directly:

```text
C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

This avoids conflicts with Visual Studio Developer PowerShell, which may set `VCPKG_ROOT` to Visual Studio's internal vcpkg directory.

Install the required Windows dependencies from the repository root with manifest mode:

```powershell
cd C:\path\to\Perastage
C:\vcpkg\vcpkg.exe install --triplet x64-windows
C:\vcpkg\vcpkg.exe install "gettext[tools]:x64-windows"
```

The manifest pins the vcpkg baseline and requests `wxwidgets[secretstore]`, which enables `wxUSE_SECRETSTORE` for Windows Credential Manager integration. If an existing vcpkg tree was built without the feature, repair it explicitly with:

```powershell
C:\vcpkg\vcpkg.exe install "wxwidgets[secretstore]:x64-windows" --recurse
```

After changing wxWidgets features, delete the affected Perastage build directory and reconfigure so the CMake secure-store probe uses the rebuilt wxWidgets configuration.

Gettext tools are build-time dependencies for localization catalog generation. They are not Perastage runtime dependencies. Homebrew gettext is keg-only on macOS; add `$(brew --prefix gettext)/bin` to `PATH` before configuring CMake so `msgfmt`, `xgettext`, `msgmerge`, and `msgattrib` resolve consistently.

If your vcpkg installation is not in `C:/vcpkg`, use one of these options:

1. Edit the Windows `CMAKE_TOOLCHAIN_FILE` values in `CMakePresets.json`.
2. Create a local `CMakeUserPresets.json` file that overrides `CMAKE_TOOLCHAIN_FILE`.

`CMakeUserPresets.json` is local to each developer machine and should not be committed.

## CMake presets strategy

Perastage uses CMake presets for repeatable local and CI builds.

```text
CMakePresets.json
CMakeUserPresets.json
```

`CMakePresets.json` is the shared project-level preset file and is tracked in Git.

`CMakeUserPresets.json` is local to each developer machine. Use it only when local toolchain paths or environment overrides are needed.

## Optional local Windows user presets

If your vcpkg installation is not located at `C:/vcpkg`, create a local file named:

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
        "CMAKE_TOOLCHAIN_FILE": "D:/tools/vcpkg/scripts/buildsystems/vcpkg.cmake"
      }
    },
    {
      "name": "local-win-x64-release-ninja",
      "displayName": "Local Windows x64 Release (Ninja)",
      "description": "Local Release Ninja build using a custom vcpkg installation.",
      "inherits": "win-x64-release-ninja",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "D:/tools/vcpkg/scripts/buildsystems/vcpkg.cmake"
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

For the recommended `C:/vcpkg` setup, this local file is not required.

## Visual Studio workflow on Windows

For the standard Windows setup, install vcpkg in `C:/vcpkg` and use the shared Windows presets directly.

Typical setup:

1. Install vcpkg in `C:/vcpkg`.
2. Install the required vcpkg dependencies from the manifest, including `wxwidgets[secretstore]`.
3. Open the repository folder in Visual Studio.
4. Select `Local Machine`.
5. Select a Windows configure preset such as `Windows x64 Debug (Ninja)`.
6. Select a Windows build preset such as `Build Windows Debug (Ninja)`.

If Visual Studio shows stale configuration errors after changing presets, close Visual Studio and remove the local `.vs` folder and the affected build directory before configuring again.

## Command-line build

List available presets:

```powershell
cmake --list-presets
```

Configure a Windows Debug Ninja build:

```powershell
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

then Visual Studio is using its own vcpkg instance instead of the recommended `C:/vcpkg` installation. Make sure the updated `CMakePresets.json` is being used, clear the CMake cache, and remove stale `.vs` or build folders if needed.

## Secure credential-store verification

Official Windows and macOS presets require `PERASTAGE_REQUIRE_SECURE_CREDENTIAL_STORE=ON`. Official Linux packaging workflows also pass this option explicitly. When the option is enabled, CMake compiles a small `wx/setup.h` probe and fails if `wxUSE_SECRETSTORE` is disabled. On Linux, building wxWidgets with this feature requires libsecret development headers such as `libsecret-1-dev`; runtime persistence still depends on a running Freedesktop Secret Service provider such as GNOME Keyring or KWallet.

For a manual Windows functional check after rebuilding dependencies:

1. Save valid GDTF Share credentials in Perastage.
2. Restart Perastage.
3. Open the GDTF download workflow.
4. Confirm no secure-storage persistence warning appears.
5. Confirm credentials are not requested again solely because the application restarted.
6. Confirm the test entry is present in Windows Credential Manager.
