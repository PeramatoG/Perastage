# Perastage Feature Overview

This document summarizes the main end-user capabilities in Perastage. For exact parser behavior and format compliance details, follow the linked policy documents.

## Project and Scene Management

- Open and save Perastage projects (`.pstg`) with multiple named pages/layouts.
- Organize fixtures, trusses, hoists, and objects into layers with per-layer visibility and selection control.
- Preserve workspace preferences such as view mode, directories, and window layout.

## MVR and GDTF Workflows

- Import and export MVR scenes for interoperability with other lighting tools.
- Use a built-in fixture dictionary mapped to GDTF files stored in `library/`.
- Download fixtures from GDTF-Share through the GUI.
- Handle GDTF filename collisions during export with deterministic renaming.
- Follow [GDTF mutation policy](gdtf_mutation_policy.md) for `description.xml` writes, revision behavior, and schema fallback.

## Rider and Text-to-Scene Import

- Parse rider-like text or PDF content using **Tools → Create from text**.
- Use filter-first workflows before object creation.
- Support common hang tokens and side mappings in parser input.
- Maintain parser contract in [Text-to-scene rules](text_to_scene_rules.md).

## Patch and Data Table Tools

- Manage DMX universes, addresses, and patch assignments.
- Use auto patch to assign channels by grouped fixture logic.
- Edit fixtures, trusses, hoists, and objects in dedicated tables with batch operations.

## Visualization and Layout

- Use a real-time OpenGL 3D viewer for scene navigation and inspection.
- Use the 2D viewer for plan-oriented documentation and vector capture workflows.
- Build multi-page layouts with views, legends, event tables, text blocks, and images.

## Print and Export Outputs

- Print layouts and tables to PDF/CSV-oriented outputs.
- Export fixture/truss/object assets through tool workflows.
- Generate documentation sheets from layout pages.

## GUI and Workflow Notes

- Some tools are build-gated to keep Release builds focused on production workflows.
- Unit systems for distance and weight are configurable in Preferences.
- Shortcut scope and precedence are documented in [GUI shortcut architecture](gui_shortcut_architecture.md).

## Known Limitations

- Some advanced menu tools remain incomplete or workflow-specific.
- Very large scenes may stress rendering and redraw paths.
- Interchange support is centered on MVR; additional formats are limited.
- Undo/redo coverage is broad but not universal across all operations.
