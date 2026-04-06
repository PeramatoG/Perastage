# Build and Dependency Guide

This document covers essential and advanced build details for Perastage. Use it together with `CMakePresets.json` when available in your environment.

## Core Requirements

- CMake 3.21 or newer.
- C++20-capable toolchain.
- Main dependencies: wxWidgets, tinyxml2, OpenGL/GLU, GLEW, CURL, nanovg, and PoDoFo.

## Quick Build

```bash
cmake -S . -B build
cmake --build build --config Release
```

### Running Tests

```bash
ctest --test-dir build
```

## Build Outputs

- The app executable is generated in the selected build output folder.
- The `perastage_stage` target prepares runtime assets in `out/install/<CONFIG>` for packaging workflows.
- The `perastage_symbols` target can collect debug symbols on supported Windows setups.

## Optional Peraviz and DMX Options

Perastage includes optional Peraviz integration and DMX/Art-Net support.

```bash
cmake -S . -B build -DPERAVIZ_ENABLE_NATIVE=ON -DPERAVIZ_ENABLE_DMX=ON
```

Use these options only when your environment has the required dependencies and intended runtime support.

## Project File Format

Perastage project files (`.pstg`) are ZIP archives that typically include:

- `config.json` for preferences and layout-related settings.
- `scene.mvr` for show data representation.

Standard ZIP tools can inspect archive contents for diagnostics.

## Related Documents

- [Windows installation notes](installation_windows.md)
- [Packaging and platform integration](packaging.md)
- [Troubleshooting](troubleshooting.md)
