# Perastage: high-level repository map

> **Scope (high-level view):** this document summarizes the main modules and submodules only. For full file-level detail, use `rg --files` or your IDE tree view.

This map is aligned with the terminology used in `README.md` and `docs/architecture.md`: functional modules (`core`, `gui`, `viewer2d`, `viewer3d`, `models`, `mvr`), packaged runtime content (`library`, `resources`), vendored dependencies (`third_party`), and tests/docs (`tests`, `docs`).

## Top-level structure

```text
Perastage/
├── main.cpp                     # wxWidgets application entry point.
├── CMakeLists.txt               # Root build orchestration and global dependencies.
├── README.md                    # Product overview and repository layout.
├── docs/architecture.md         # Architecture and repository conventions.
├── core/                        # Shared business logic and cross-cutting services.
│   ├── layouts/                 # Printable layout/page management.
│   └── print/                   # Printing and PDF/table export helpers.
├── gui/                         # wxWidgets UI (main window, panels, dialogs).
│   ├── mainwindow/              # Main-window workflows, IDs, and controllers.
│   └── fixturetable/            # Fixture-table parsing, editing, and column logic.
├── viewer2d/                    # 2D renderer and export-related utilities.
│   └── pdf/                     # PDF encoding/writing and drawing primitives.
├── viewer3d/                    # 3D renderer, camera, loaders, and pipeline.
│   ├── render/                  # Render passes and OpenGL pipeline.
│   ├── culling/                 # Visibility and bounds systems.
│   ├── picking/                 # Selection and interaction systems.
│   ├── labels/                  # Label rendering systems.
│   ├── resources/               # Render-resource synchronization.
│   └── interfaces/              # Render/selection context contracts.
├── models/                      # Scene data structures (fixtures/trusses/hoists/objects/layers).
├── mvr/                         # MVR format import/export modules.
├── tests/                       # Automated tests and lightweight checks.
├── library/                     # Packaged runtime content (fixtures, trusses, etc.).
├── resources/                   # Visual/platform resources (icons, fonts, .rc).
├── third_party/                 # Vendored third-party headers.
├── licenses/                    # Third-party license files.
└── peraviz/                     # Godot-based sandbox/prototype + native extension.
```

## Modules and responsibilities

- **`core/`**: project/config services, rider/PDF import helpers, auto-patch logic, and persistence/export utilities.
- **`gui/`**: main UI composition and editing/visualization tools (tables, panels, dialogs, menus).
- **`viewer2d/`**: 2D plan visualization and command/resource generation for printing/export.
- **`viewer3d/`**: real-time 3D visualization, geometry loading (GDTF/3DS/GLB), and render pipeline.
- **`models/`**: core scene model (fixtures, trusses, hoists/supports, objects, layers).
- **`mvr/`**: inbound/outbound integration with the MVR ecosystem.

## Critical files (explicit exception to high-level granularity)

- `main.cpp`: application bootstrap.
- `CMakeLists.txt`: primary build orchestration.
- `README.md`: functional/documentation reference.
- `docs/architecture.md`: repository structure conventions.

## Maintenance guidance

- Keep module/submodule names consistent with `README.md` and `docs/architecture.md`.
- Avoid listing all individual files except truly critical entry points.
- If repository layout changes, update this document in the same PR.
