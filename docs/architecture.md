# Architecture and repository conventions

This document defines the expected directory conventions for Perastage.

## Top-level layout

- `core/`: shared business logic and services.
- `gui/`: wxWidgets UI and main window workflows.
- `viewer2d/`: 2D renderer and PDF/export helpers.
- `viewer3d/`: 3D renderer, loaders and render passes.
- `models/`: core scene data structures.
- `mvr/`: MVR import/export modules.
- `third_party/`: vendored third-party single-header dependencies (for example `json.hpp`, `stb_easy_font.h`).
- `library/`: bundled runtime content (fixtures, trusses, `scene_objects`, examples).

## Third-party convention

- Keep vendored code under `third_party/` only.
- Do not place third-party libraries inside feature modules (`core/`, `gui/`, `viewer*`, etc.).
- Prefer package-managed dependencies for compiled libraries; use `third_party/` for vendored sources/headers only.

## CMake convention

- Root `CMakeLists.txt` owns target creation and global dependencies.
- Feature directories contribute sources using local `CMakeLists.txt` files and `target_sources(${PROJECT_NAME} ...)`.
- Avoid `file(GLOB_RECURSE ...)` for project source registration; list files explicitly.
- Keep include directories close to the module that owns them.

## Library convention

- Scene-object presets live in `library/scene_objects/`.
- Any new code/path references must use `scene_objects` (underscore), not `scene objects`.
