# Build and Dependency Guide

This document covers baseline and advanced build behavior for Perastage. It is the detailed companion to the short installation section in `README.md`.

## Core Requirements

- CMake 3.21 or newer.
- C++20-capable compiler/toolchain.
- Required libraries include wxWidgets, tinyxml2, OpenGL/GLU, GLEW, CURL, nanovg, and PoDoFo.

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

## Optional Peraviz and DMX/Art-Net Integration

Perastage can build optional Peraviz components when dependencies are present.

```bash
cmake -S . -B build -DPERAVIZ_ENABLE_NATIVE=ON -DPERAVIZ_ENABLE_DMX=ON
```

### Notes

- These options are not required for core Perastage workflows.
- Ensure Python 3 and dependent toolchain pieces are discoverable when enabling native Peraviz builds.

## Project File Format

Perastage project files (`.pstg`) are ZIP archives that commonly include:

- `config.json` for preferences and layout/user settings,
- `scene.mvr` for show-scene payload.

You can inspect these archives with standard ZIP tools for diagnostics.

## Related Documents

- [Windows installation notes](installation_windows.md)
- [Packaging and platform integration](packaging.md)
- [Build troubleshooting](troubleshooting.md)
