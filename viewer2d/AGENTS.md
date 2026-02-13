# AGENTS.md (viewer2d)

## Scope
These rules apply to everything under `viewer2d/`.

## Rules
1. **Contain hotspots**
   - Files to monitor:
     - `viewer2dpanel.cpp`
     - `pdf/layout_pdf_exporter.cpp`
   - Add new capabilities using responsibility-focused components (e.g., camera, grid/overlay, page composition, element renderers).

2. **Suggested split direction when touching hotspots**
   - `viewer2dpanel.cpp`: separate camera/grid/overlay logic from draw submission.
   - `layout_pdf_exporter.cpp`: separate page composition from element rendering.

3. **Incremental refactoring**
   - Do not attempt full rewrites; extract only around the modified flow.
