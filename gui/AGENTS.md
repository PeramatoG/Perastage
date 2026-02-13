# AGENTS.md (gui)

## Scope
These rules apply to everything under `gui/`.

## Rules
1. **Do not grow UI hotspots without prior extraction**
   - Priority files to contain growth:
     - `layoutviewerpanel.cpp`
     - `mainwindow_menu.cpp`
     - `fixturetablepanel.cpp`
   - New menu logic, routing, or complex behavior should go into specialized files (`*_menu*`, `*_controller*`, `*_layout*`, etc.).

2. **UI responsibility separation**
   - Keep these concerns separated:
     - widget/layout construction,
     - event wiring,
     - command/action logic,
     - UI IO/import/export.

3. **Large changes in hotspots**
   - For large hotspot changes, add tests/regressions (or available integration checks) for touched behavior first, then extract.

4. **Dependencies**
   - Avoid introducing GUI dependencies on internal details of other modules when a public API or helper exists in the owning module.
