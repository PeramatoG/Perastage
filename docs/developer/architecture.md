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

- Root `CMakeLists.txt` owns project options, principal target creation, shared target configuration, and module orchestration. Focused modules own dependency discovery (`PerastageDependencies.cmake`), localization (`PerastageLocalization.cmake`), runtime staging (`PerastageRuntimeStaging.cmake`), installation (`PerastageInstall.cmake`), and packaging (`PerastagePackaging.cmake`).
- `cmake/platform/PerastagePlatform.cmake` dispatches target-level platform configuration to separate Windows, macOS, and Linux owners after the application target exists. Linux desktop, MIME, and icon integration remains installation configuration rather than target configuration.
- Every top-level application source module contributes its explicit source list using its local `CMakeLists.txt` and `target_sources(${PROJECT_NAME} ...)`.
- `docs/developer/repository_structure_baseline.json` is the authoritative machine-readable contract for source-module classification and CMake registration.
- Avoid recursive or wildcard project-source discovery; list files explicitly.
- Keep include directories close to the module that owns them.

### Application include-directory ownership

The ORG-023 audit classified the application target's include directories from
actual include spellings and CMake ownership before ORG-024 changed their
declarations:

| Classification | Audited paths | Evidence and ownership |
|---|---|---|
| A: module-owned and formerly redundant in root | `core`, `core/layouts`, `core/print`; `gui`, `gui/mainwindow/controllers`, `gui/mainwindow/ids`; `models`; `mvr`; `viewer2d`, `viewer2d/pdf`; `viewer3d`, `viewer3d/interfaces`, `viewer3d/resources`, `viewer3d/culling`, `viewer3d/labels`, `viewer3d/picking`, `viewer3d/render` | Sources use both unqualified headers such as `logger.h` and nested spellings such as `resources/resource_sync_system.h`. Each complete group was already applied to `${PROJECT_NAME}` by its owning module CMake file, so ORG-024 removed only the duplicate root declarations. |
| B: module-specific without local ownership | None | All audited feature paths had an active owner declaration. `viewer_common` was locally owned even though it was not in the pre-audit root list. |
| C: shared project-wide | `third_party` | The application uses unqualified vendored headers from several modules: `json.hpp` in Core, GUI, MVR, Viewer2D, and Viewer3D, and `stb_easy_font.h` in Viewer3D. The shared application declaration remains in root. |
| D: dependency-provided state | `${_wx_includes}`, `${NANOVG_INCLUDE_DIR}` | Dependency configuration supplies these compatibility variables and application sources consume wxWidgets across modules and NanoVG in the viewers. They remain shared target configuration pending a separate dependency-modernization effort. |
| E: unresolved compatibility exposure | `resources` | No application C/C++ header include currently resolves through this directory; it contains platform resources and runtime assets. The include is retained until the same revision has native Windows, Linux, and macOS build evidence that it is unnecessary. |

Test executables are separate targets and continue to declare their own focused
include requirements in `tests/CMakeLists.txt`; their requirements did not
justify any application-target path. The audit also observed factual
cross-module header use (notably GUI and viewers consuming Core and Models,
Viewer3D consuming MVR data, and GUI consuming Viewer2D/Viewer3D facilities).
ORG-025 will evaluate dependency direction; this audit defines no direction
policy.

All feature modules still contribute to the same `${PROJECT_NAME}` target.
Moving declarations to module CMake files records ownership and reduces root
coupling, but does not create source-local compiler isolation or a new target
boundary. Stronger compile-time isolation would require a future target-level
architecture change.

## Library convention

- Scene-object presets live in `library/scene_objects/`.
- Any new code/path references must use `scene_objects` (underscore), not `scene objects`.

## GUI architecture references

- Keyboard shortcut routing and scope rules are documented in `docs/developer/../developer/gui_shortcut_architecture.md`.
- Storage source-of-truth and runtime precedence are documented in `docs/developer/storage_policy.md`.
