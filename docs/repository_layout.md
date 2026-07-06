# Repository Layout

This page provides a concise map of the main Perastage repository areas. For the broader high-level tree, see [perastage_tree.md](perastage_tree.md). For architectural boundaries and contribution rules, see [Architecture](architecture.md).

## Top-level structure

| Path | Purpose |
|------|---------|
| `main.cpp` | wxWidgets application entry point and top-level initialization. |
| `CMakeLists.txt` | Root build orchestration, global dependencies, platform options, install rules, and module registration. |
| `CMakePresets.json` | Supported configure/build presets for local development workflows. |
| `cmake/` | CMake helper scripts, generated configuration templates, and platform metadata templates. |
| `core/` | Core logic, import helpers, dictionaries, patching, layouts, printing, persistence, and shared services. |
| `gui/` | wxWidgets windows, dialogs, menus, panels, UI controllers, and user interaction workflows. |
| `models/` | Scene data models for fixtures, trusses, hoists, objects, layers, and placement data. |
| `mvr/` | MVR importer/exporter implementation and MVR-xchange networking modules. |
| `viewer2d/` | 2D plan view rendering, layout capture, PDF/export support, and 2D viewer utilities. |
| `viewer3d/` | 3D rendering engine, camera/view tooling, picking, labels, culling, and render-resource synchronization. |
| `viewer_common/` | Shared viewer utilities used by the viewer modules to avoid duplicating cross-view logic. |
| `library/` | Bundled fixture, truss, object, dictionary, and example runtime data. |
| `resources/` | Icons, images, fonts, Windows resource files, and runtime visual resources. |
| `packaging/` | Installer, desktop integration, package metadata, and staging helpers. |
| `third_party/` | Vendored third-party headers or small external code components kept in the repository. |
| `licenses/` | Third-party license files. |
| `tests/` | Unit tests, architecture guard scripts, and repository consistency checks. |
| `docs/` | User documentation, technical notes, architecture policies, and documentation website assets. |
| `.github/workflows/` | GitHub Actions workflows for CI, packaging, release assets, and platform builds. |

## Build module registration

The root CMake file explicitly registers the main source modules with `add_subdirectory(...)` instead of discovering project sources recursively. The currently registered source modules are:

```text
core/
gui/
viewer2d/
viewer3d/
viewer_common/
```

Keep this list aligned with `CMakeLists.txt` whenever a new top-level source module is introduced.

## Documentation layout

Perastage documentation is intentionally split by audience and responsibility:

| Path | Purpose |
|------|---------|
| `README.md` | Short project overview, highlights, and entry links. |
| `help.md` | In-app help content. |
| [perastage_tree.md](perastage_tree.md) | High-level repository map used by architecture guard scripts. |
| `docs/build.md` | Build requirements, dependency setup, and local CMake workflows. |
| `docs/packaging.md` | Release packaging, installers, desktop integration, and platform distribution notes. |
| `docs/troubleshooting.md` | Known failure modes and practical fixes. |
| `docs/documentation_policy.md` | Documentation organization and synchronization rules. |
| `docs/architecture.md` | Architecture boundaries and project structure conventions. |
| `docs/assets/` | Assets used by the documentation website. |
| `docs/*.html` | Static documentation website entry points and shells. |
| `docs/*.md` | Markdown documentation sources and technical guides. |

Avoid duplicating long sections across documentation files. Prefer one source of truth and link to it from related pages.

## Related documents

- [perastage_tree.md](perastage_tree.md)
- [Architecture](architecture.md)
- [Documentation Policy](documentation_policy.md)
- [Build and dependency guide](build.md)
- [Packaging and Platform Integration](packaging.md)
