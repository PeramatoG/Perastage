# Build and Dependency Guide

This document covers baseline and advanced build behavior for Perastage. It is the detailed companion to the short installation section in `README.md`.

## Core Requirements

- CMake 3.21 or newer.
- C++20-capable compiler/toolchain.
- Required libraries include wxWidgets, tinyxml2, OpenGL/GLU, GLEW, CURL, nanovg, PoDoFo, and meshoptimizer.

### Dependency note for vcpkg users

If you install dependencies with vcpkg (classic mode), make sure `meshoptimizer` is included in the package list together with the existing Perastage dependencies.

## Quick Build (All Platforms)

```bash
cmake -S . -B build
cmake --build build --config Release
```

## Presets and Configuration Strategy

When available, prefer `CMakePresets.json` for consistent local/team builds.

- Single-config generators rely on `CMAKE_BUILD_TYPE`.
- Multi-config generators (for example Visual Studio) use `--config Debug|Release`.

## Running Tests

```bash
ctest --test-dir build
```

## Build Targets and Outputs

- Application target: `Perastage` executable.
- `perastage_stage`: stages runtime files in `out/install/<CONFIG>` for packaging.
- `perastage_symbols`: collects symbol artifacts on supported Windows environments.
