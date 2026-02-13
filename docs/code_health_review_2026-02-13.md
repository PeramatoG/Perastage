# Code health review (2026-02-13)

## Scope

This review checks whether Perastage is now in a solid state of cleanliness/order to continue feature work.

What was reviewed:

- Repository/module structure and consistency against the project conventions.
- Build and architecture conventions in root and module CMake files.
- Existing guard scripts under `tests/` for architectural drift.
- Current hotspots (very large files) that may still deserve incremental refactors.

## What is in a good place

1. **Repository layout is coherent and documented.**
   - The top-level map and module responsibilities are clearly defined in `perastage_tree.md` and aligned with `docs/architecture.md`.
   - Main domains (`core`, `gui`, `viewer2d`, `viewer3d`, `models`, `mvr`) are separated cleanly.

2. **Build organization follows modular conventions.**
   - Root `CMakeLists.txt` delegates sources to module CMake files with `add_subdirectory(...)`.
   - The architecture convention explicitly avoids recursive globbing and keeps source ownership explicit.

3. **Architectural guardrails exist and pass.**
   - `tests/check_perastage_tree_modules.sh` passes.
   - `tests/check_no_configmanager_get_in_gui.sh` passes.

4. **Prior decomposition work is visible.**
   - Example: GUI main-window logic is split across focused files (`mainwindow.cpp`, `mainwindow_menu.cpp`, `mainwindow_layout.cpp`, `mainwindow_io.cpp`) and controller/ID subfolders.
   - Example: layout-viewer related logic is split into multiple translation units (`layoutviewerpanel.cpp`, `_legend`, `_eventtable`, `_text`, `_image`, `_view`).

## Remaining hotspots (non-blocking)

The codebase is in a **good enough state to continue implementing features**. Still, a few large files remain likely maintenance hotspots:

- `gui/layoutviewerpanel.cpp` (~2358 LOC)
- `viewer2d/viewer2dpanel.cpp` (~1811 LOC)
- `viewer2d/pdf/layout_pdf_exporter.cpp` (~1717 LOC)
- `viewer3d/viewer3dcontroller.cpp` (~1658 LOC)
- `mvr/mvrexporter.cpp` (~1343 LOC)
- `gui/mainwindow_menu.cpp` (~1295 LOC)
- `gui/fixturetablepanel.cpp` (~1225 LOC)

These are not necessarily "bad" by themselves, but they are the most likely places where future features can reintroduce coupling and regressions.

## Recommendation for next phase

### Decision

**Yes: the project is currently in a professional and workable state to proceed with new features.**

### Lightweight rules to keep it healthy

1. **Use a soft file-size guardrail for new work.**
   - Prefer adding new behavior in adjacent helper/service files instead of growing hotspot files.
   - If a file approaches ~1200–1500 LOC and spans multiple responsibilities, split before adding major features.

2. **Keep enforcing architecture checks in CI.**
   - Keep existing `tests/check_*.sh` scripts mandatory in CI.
   - Add new small guard scripts when a new architectural boundary is introduced.

3. **Refactor hotspots opportunistically, not with a big-bang rewrite.**
   - Extract by responsibility (input handling, rendering orchestration, serialization, menu wiring).
   - Refactor only around touched code paths when implementing features.

4. **Protect large-module behavior with targeted tests first.**
   - Before deep splits in hotspot modules, add regression tests around current behavior.

## Suggested incremental backlog (optional)

- `layoutviewerpanel.cpp`: extract interaction state machine (selection/drag/resize/context-menu routing).
- `viewer2dpanel.cpp`: separate camera/grid/overlay logic from draw submission.
- `layout_pdf_exporter.cpp`: split page composition vs element renderers.
- `viewer3dcontroller.cpp`: split command handling from scene/resource synchronization.
- `mvrexporter.cpp`: separate data normalization from XML/package writing.

## Summary

Perastage looks **organized, coherent and maintainable enough** to keep building new capabilities now. The right strategy from here is **incremental containment** of the remaining large hotspots while continuing feature delivery.
