# AGENTS.md (global)

## Objective
Keep Perastage clean and modular while continuing to deliver new features, following `docs/code_health_review_2026-02-13.md`.

## Mandatory global rules

1. **Modularity before growing large files**
   - Avoid adding large logic blocks to hotspot files.
   - If a change introduces a new responsibility, create/use adjacent files organized by responsibility.

2. **File-size guardrail (soft limit)**
   - When touching a file near **1200–1500 LOC**, prioritize extraction before adding major features.
   - If extraction is not feasible in the same PR, include a short technical note in the PR description with the next recommended split.

3. **Architecture and build conventions**
   - Keep explicit source ownership in CMake (no `GLOB_RECURSE` for project source registration).
   - Respect module boundaries described in `docs/architecture.md` and `perastage_tree.md`.

4. **Mandatory checks before closing changes**
   - Run and keep green:
     - `tests/check_perastage_tree_modules.sh`
     - `tests/check_no_configmanager_get_in_gui.sh`
   - If a new architectural boundary is introduced, add a small `tests/check_*.sh` script in the same PR.

5. **Refactoring strategy**
   - Refactor incrementally around touched code paths.
   - Avoid big-bang rewrites.

6. **Language policy for contributions**
   - All new/updated code must be written in English.
   - All code comments and documentation updates should be written in English.

## Current hotspots (watch for growth)
- `gui/layoutviewerpanel.cpp`
- `viewer2d/viewer2dpanel.cpp`
- `viewer2d/pdf/layout_pdf_exporter.cpp`
- `viewer3d/viewer3dcontroller.cpp`
- `mvr/mvrexporter.cpp`
- `gui/mainwindow_menu.cpp`
- `gui/fixturetablepanel.cpp`

## Change quality
- Keep changes small, focused, and explicit in responsibility naming.
- Before introducing cross-module coupling, prefer an interface/helper in the module that owns the responsibility.
