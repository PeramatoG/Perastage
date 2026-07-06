# Build and Dependency Guide

This document covers baseline and advanced build behavior for Perastage. It is the detailed companion to the short installation section in `README.md`.

## Core Requirements

- CMake 3.21 or newer.
- C++20-capable compiler/toolchain.
- wxWidgets 3.3.1 or compatible development package.
- Required libraries:
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

## vcpkg dependency setup

Perastage can be built with vcpkg in classic mode. On Windows, the recommended local convention is to install vcpkg in:

```text
C:/vcpkg
```

Install the required Windows dependencies with:

```powershell
C:\vcpkg\vcpkg.exe install tinyxml2:x64-windows curl:x64-windows glew:x64-windows meshoptimizer:x64-windows nanovg:x64-windows podofo:x64-windows zlib:x64-windows backward-cpp:x64-windows mdns:x64-windows
```

The shared project presets use `VCPKG_ROOT` so the repository does not hard-code a developer-specific path.

On a normal PowerShell session, `VCPKG_ROOT` should point to the vcpkg installation used for Perastage:

```powershell
[Environment]::SetEnvironmentVariable("VCPKG_ROOT", "C:\vcpkg", "User")
```

Then close and reopen terminals and IDEs so they pick up the updated environment.

## CMake presets strategy

Perastage uses CMake presets for repeatable local and CI builds.

```text
CMakePresets.json
CMakeUserPresets.json
```

`CMakePresets.json` is the shared project-level preset file and is tracked in Git.

`CMakeUserPresets.json` is local to each developer machine and should not be committed. Use it when a local toolchain path or environment override is needed.

This is especially useful on Windows because Visual Studio Developer PowerShell may set `VCPKG_ROOT` to Visual Studio's internal vcpkg directory instead of the vcpkg installation used for Perastage.

## Local Windows user presets

Create a local file named:

```text
CMakeUserPresets.json
```

in the repository root.

This file should not be committed. It should contain local machine settings only.

Example for a Windows machine using vcpkg in `C:/vcpkg`:

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "local-win-x64-debug-ninja",
      "displayName": "Local Windows x64 Debug (Ninja)",
      "description": "Local Debug Ninja build using the developer vcpkg installation.",
      "inherits": "win-x64-debug-ninja",
      "environment": {
        "VCPKG_ROOT": "C:/vcpkg"
      }
    },
    {
      "name": "local-win-x64-release-ninja",
      "displayName": "Local Windows x64 Release (Ninja)",
      "description": "Local Release Ninja build using the developer vcpkg installation.",
      "inherits": "win-x64-release-ninja",
      "environment": {
        "VCPKG_ROOT": "C:/vcpkg"
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

If your vcpkg installation is in another location, adjust the `VCPKG_ROOT` value.

For example:

```json
"VCPKG_ROOT": "D:/tools/vcpkg"
```

## Visual Studio workflow on Windows

For the Visual Studio IDE workflow, prefer the local presets from `CMakeUserPresets.json`.

Typical setup:

1. Install vcpkg dependencies.
2. Create `CMakeUserPresets.json` in the repository root.
3. Set the local `VCPKG_ROOT` value inside `CMakeUserPresets.json`.
4. Open the repository folder in Visual Studio.
5. Select `Local Machine`.
6. Select a local configure preset such as `Local Windows x64 Debug (Ninja)`.
7. Select a local build preset such as `Local Build Windows Debug (Ninja)`.

This keeps the public repository portable while still allowing each development machine to define its own vcpkg path.

## Command-line build

After the environment is configured, list available presets:

```powershell
cmake --list-presets
```

Configure a Windows Debug Ninja build using the local user preset:

```powershell
cmake --preset local-win-x64-debug-ninja
```

Build it:

```powershell
cmake --build --preset local-win-debug-build-ninja
```

Configure a Windows Release Ninja build using the local user preset:

```powershell
cmake --preset local-win-x64-release-ninja
```

Build it:

```powershell
cmake --build --preset local-win-release-build-ninja
```

If you want to use the shared preset directly from a Developer PowerShell that overrides `VCPKG_ROOT`, set the process variable first:

```powershell
$env:VCPKG_ROOT = "C:\vcpkg"
cmake --preset win-x64-debug-ninja
cmake --build --preset win-debug-build-ninja
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

For a Windows local Debug Ninja build:

```powershell
ctest --test-dir build/local-win-x64-debug-ninja --output-on-failure
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
Could NOT find ZLIB (missing: ZLIB_LIBRARY ZLIB_INCLUDE_DIR)
```

or a similar error for `tinyxml2`, `CURL`, `GLEW`, `meshoptimizer`, `nanovg`, `podofo`, `Backward`, or `mdns`, the most common cause is that CMake is using a different vcpkg installation than the one where Perastage dependencies were installed.

First verify that the dependency is installed in the intended vcpkg instance. For example:

```powershell
C:\vcpkg\vcpkg.exe install zlib:x64-windows
```

Then verify which vcpkg instance CMake is using:

```powershell
echo $env:VCPKG_ROOT
Test-Path "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
Test-Path "$env:VCPKG_ROOT\installed\x64-windows\include\zlib.h"
```

The expected `VCPKG_ROOT` value depends on your local setup. On the recommended Windows setup it is:

```text
C:\vcpkg
```

Visual Studio Developer PowerShell may set `VCPKG_ROOT` to Visual Studio's internal vcpkg directory instead. In that case, CMake may fail to find dependencies even if they are installed in `C:/vcpkg`.

For Visual Studio IDE builds, prefer a local `CMakeUserPresets.json` preset that sets the intended vcpkg root.

For command-line builds from a shell where `VCPKG_ROOT` has been overridden, set the process variable before configuring:

```powershell
$env:VCPKG_ROOT = "C:\vcpkg"
cmake --preset win-x64-debug-ninja
```
