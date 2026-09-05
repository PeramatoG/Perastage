# Perastage: high-level repository map

> **Scope (high-level view):** this document summarizes the main modules and submodules only. For full file-level detail, use `rg --files` or your IDE tree view.

This map is aligned with the terminology used in `README.md` and `docs/developer/architecture.md`: functional modules (`core`, `gui`, `viewer2d`, `viewer3d`, `viewer_common`, `models`, `mvr`), packaged runtime content (`library`, `resources`), packaging and build support (`cmake`, `packaging`, `.github/workflows`), vendored dependencies (`third_party`), and tests/docs (`tests`, `docs`).

## Top-level structure

```text
Perastage/
|-- main.cpp                     # wxWidgets application entry point.
|-- CMakeLists.txt               # Root target creation, build orchestration, and module registration.
|-- CMakePresets.json            # Supported local configure/build presets.
|-- README.md                    # Product overview, features, and entry links.
|-- help.md                      # In-app help content.
|-- docs/                        # User documentation, architecture notes, repository map, and docs website assets.
|-- AGENTS.md                    # Repository guidance for automated coding agents.
|-- VERSION                      # Single project version source.
|-- cmake/                       # Dependency, localization, and runtime-staging build modules, templates, and helper scripts.
|-- core/                        # Shared business logic and cross-cutting services.
|   |-- layouts/                 # Printable layout/page management.
|   `-- print/                   # Printing and PDF/table export helpers.
|-- gui/                         # wxWidgets UI (main window, panels, dialogs).
|   |-- mainwindow/              # Main-window workflows, IDs, and controllers.
|   `-- fixturetable/            # Fixture-table parsing, editing, and column logic.
|-- viewer2d/                    # 2D renderer and export-related utilities.
|   `-- pdf/                     # PDF encoding/writing and drawing primitives.
|-- viewer3d/                    # 3D renderer, camera, loaders, and pipeline.
|   |-- render/                  # Render passes and OpenGL pipeline.
|   |-- culling/                 # Visibility and bounds systems.
|   |-- picking/                 # Selection and interaction systems.
|   |-- labels/                  # Label rendering systems.
|   |-- resources/               # Render-resource synchronization.
|   `-- interfaces/              # Render/selection context contracts.
|-- viewer_common/               # Shared viewer utilities used across viewer modules.
|-- models/                      # Scene data structures (fixtures/trusses/hoists/objects/layers).
|   `-- CMakeLists.txt           # Explicit model source registration for the application target.
|-- mvr/                         # MVR format import/export modules and MVR-xchange networking.
|   `-- CMakeLists.txt           # Explicit MVR and MVR-xchange source registration.
|-- packaging/                   # Installer, desktop integration, and package metadata.
|-- tests/                       # Automated tests and lightweight checks.
|-- library/                     # Packaged runtime content (fixtures, trusses, etc.).
|-- resources/                   # Visual/platform resources (icons, fonts, .rc).
|-- third_party/                 # Vendored third-party headers.
|-- licenses/                    # Third-party license files.
`-- .github/workflows/           # GitHub Actions workflows.
```

## Modules and responsibilities

- **`core/`**: project/config services, rider/PDF import helpers, auto-patch logic, layout/print support, and persistence/export utilities.
- **`gui/`**: main UI composition and editing/visualization tools (tables, panels, dialogs, menus).
- **`viewer2d/`**: 2D plan visualization and command/resource generation for printing/export.
- **`viewer3d/`**: real-time 3D visualization, geometry loading (GDTF/3DS/GLB), and render pipeline.
- **`viewer_common/`**: shared viewer-side helpers that should not belong exclusively to either the 2D or 3D viewer.
- **`models/`**: core scene model (fixtures, trusses, hoists/supports, objects, layers).
- **`mvr/`**: inbound/outbound integration with the MVR ecosystem, including MVR-xchange support.
- **`packaging/`**: platform packaging metadata, installer scripts, and desktop integration files.

## Critical files (explicit exception to high-level granularity)

- `main.cpp`: application bootstrap.
- `CMakeLists.txt`: primary build orchestration.
- `CMakePresets.json`: supported local configure/build presets.
- `README.md`: functional/documentation reference.
- `docs/developer/architecture.md`: repository structure conventions.
- `docs/developer/documentation_policy.md`: documentation organization and synchronization rules.

## Maintenance guidance

- Keep module/submodule names consistent with `README.md`, `docs/developer/architecture.md`, and `docs/developer/repository_layout.md`.
- Avoid listing all individual files except truly critical entry points.
- If repository layout changes, update this document in the same PR.
- If a new top-level source module is introduced, update the architecture guard scripts under `tests/` when appropriate.
- Every top-level application source module owns an explicit local CMake source list; the root does not normally register feature implementations, and no recursive source discovery is used.
- `repository_structure_baseline.json` is the authoritative machine-readable contract for source-module classification and root module registration.
