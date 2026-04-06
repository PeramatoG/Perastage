# Repository Layout

This page provides a quick map of the main Perastage modules. For exhaustive file-level mapping, see `perastage_tree.md`.

## Top-Level Structure

| Path | Purpose |
|------|---------|
| `main.cpp` | Application entry point and top-level initialization. |
| `core/` | Core logic, import helpers, dictionaries, patching, layout and print support logic. |
| `gui/` | wxWidgets windows, dialogs, menus, and interaction panels. |
| `models/` | Scene data models for fixtures, trusses, hoists, and objects. |
| `mvr/` | MVR importer/exporter implementation. |
| `viewer3d/` | 3D rendering engine and camera/view tooling. |
| `viewer2d/` | 2D plan view rendering and capture tools. |
| `library/` | Bundled fixture/truss/object data and example assets. |
| `resources/` | Icons, images, and runtime resources. |
| `tests/` | Unit and guard scripts. |
| `docs/` | Specifications, architecture notes, and operational documentation. |

## Related Documents

- `perastage_tree.md`
- [Architecture](architecture.md)
