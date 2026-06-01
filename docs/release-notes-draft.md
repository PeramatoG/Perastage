# Release Notes Draft

This document is a curated working draft for the next Perastage release.

It should be updated whenever a user-facing change is merged. It is not intended to be a raw changelog or a complete commit history.

Base for this draft: changes on `main` after the latest published release tag, `v1.2.0`, reviewed through the current development version `1.2.40`.

## Highlights

- Added practical fixture replacement from the Edit menu, preserving selection, placement, UUID continuity, and project color behavior where possible.
- Added measurement workflows in both 2D and 3D viewers, with toolbar toggles, live distance previews, readable labels, and status-bar feedback.
- Added update checking from the GUI with configurable startup preferences, making it easier for users to discover newer Perastage versions.
- Improved layout viewer responsiveness and reliability during project loading, import, zooming, rendering, legend resizing, and image-heavy workflows.
- Improved Windows and release packaging so draft releases attach direct installer assets instead of requiring users to unpack workflow ZIP files.

## New features

- Added a 2D measurement tool with selection/measurement toolbar controls, live preview, active-plane distance calculations, HiDPI-aware cursor mapping, and synchronized tool state when leaving the tool.
- Added a 3D measurement tool with euclidean distance display, clearer preview behavior, readable rotated labels, restart handling, and status-bar reporting.
- Added a GUI update-check action and startup update-check preference so users can manually check for updates or choose whether Perastage checks automatically.
- Added fixture replacement from the Edit menu, including improved source labeling, transform preservation, selection stability, UUID handling, and color reuse/autocolor behavior.
- Added support for packaging referenced layout images into project archives so project files remain more portable when shared or reopened elsewhere.

## Improvements

- Improved 2D layout performance when opening projects, zooming, navigating views, resizing legends, rebuilding render caches, and working with fixture labels or image-heavy layouts.
- Improved layout render progress feedback by showing clearer status-bar messages during symbol capture, texture rebuilds, legend preparation, and rendering.
- Improved GDTF model loading with better lookup ordering, GLB fallback handling, and diagnostics for missing or difficult-to-load models.
- Improved fixture symbol alignment so generated layout symbols better match each fixture's local axes.
- Improved 3D textured mesh lighting and mirrored geometry handling so dark/light patterns, ink normals, face orientation, and textured surfaces render more consistently.
- Improved release asset handling so GitHub draft releases can include direct Windows installer, Linux AppImage, and macOS DMG downloads.

## Fixes

- Fixed MVR export before saving a project so resource references are synchronized before export.
- Fixed Windows startup behavior for shell-opened MVR files, including path normalization and startup stalls while scanning for GDTF fallbacks.
- Fixed layout view freezes and busy-cursor issues caused by reentrant GUI work, render timeouts, mode changes, new project creation, and 2D view edit commits.
- Fixed layout 2D view editing so edits apply to the intended target view and reliably refresh the layout viewer afterward.
- Fixed save-time truss position persistence, including stale inactive-table transforms overwriting updated scene data.
- Fixed the truss center anchor used by the 3D measurement tool.
- Fixed GDTF download command ID collisions and added a guard to prevent duplicate MainWindow command IDs.
- Fixed MVR import and GDTF download progress UI races that could update UI from the wrong thread or after dialogs were torn down.
- Fixed MVR exports with missing non-critical resources so users receive warnings instead of failing the entire export when possible.
- Fixed 2D scene-object depth sorting so objects draw in the expected order.

## Stability and reliability

- Hardened layout rendering during MVR import to avoid redraw races, dangling progress UI callbacks, and cache rebuilds while import state is still active.
- Hardened 3D navigation diagnostics to make random navigation crashes easier to investigate.
- Added a project symbol cache manifest to reduce unnecessary symbol work and improve cache consistency, after reverting and correcting the initial implementation.
- Added automated coverage for duplicate command IDs, layout image resource registration, symbol cache manifests, and related serialization paths.

## Documentation

- Updated user documentation for update preferences, MVR export warnings, quick-start guidance, feature descriptions, and GDTF mutation policy notes.
- Updated packaging documentation to clarify preferred release asset distribution and macOS unsigned build behavior.
- Added a curated release-notes draft workflow so future public release notes can be prepared from user-friendly entries instead of raw commit lists.

## Internal changes not intended for public release notes

- Extracted several pure helper functions from large GUI and viewer files into adjacent helper modules to keep hotspot files more maintainable.
- Refactored layout render invalidation, selected element z-order mapping, label builders, and status notification plumbing without changing the intended user workflow.
- Added internal resource-reference synchronization and layout image resource registry support used by project save/export paths.
- Added symbol cache manifest infrastructure and tests; mention publicly only if it directly improves user-facing performance or reliability in the final release notes.
- Added and adjusted CI/release workflow plumbing for installer artifact handling, curated draft release notes, and version-bump safety.
