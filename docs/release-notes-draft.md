# Perastage v1.4.0 Release Notes

Changes since `v1.3.0`.

## Highlights

## New features

- Expanded basic primitive creation and editing for spheres, cubes, and cylinders with editable names, project-persistent default dimensions, default load/save buttons, and richer edit controls for position and rotation.
- Added Edit menu Group and Ungroup commands with `Ctrl+G` and `Ctrl+U` shortcuts for cross-table scene selections, preserving object placement and hang-position assignments while using MVR-compatible GroupObject hierarchy.
- Added Help menu actions to open the local logs folder and export a manual diagnostic report for troubleshooting.
- Added a startup update reminder checkbox so users can stop seeing repeated prompts for the same available version while keeping manual update checks available.

## Improvements

- Added a 2D position gizmo while dragging scene elements so movement direction feedback follows the dragged insertion point and matches the 3D viewer.
- Improved GroupObject handling so selecting a grouped member highlights and transforms the full root group, including nested groups, in 2D, 3D, and command-bar transform workflows.
- Improved group hover feedback by using a more yellow primary highlight and a paler secondary group highlight across the 3D view and related fixture, truss, hoist, and scene object table rows, matching the table selection styling.
- Improved 3D click selection for grouped scene items so a quick click selects the full group across fixture, truss, hoist, and scene object tables.
- Improved the 3D move gizmo with Blender-style cone arrowheads so drag axes are easier to distinguish from coordinate axes.

## Fixes

- Fixed scene object renaming so edited Data View names are preserved in the scene summary and MVR exports, supporting technical object-name workflows such as cable waypoints.
- Kept the 2D and 3D viewer highlights pinned to the dragged scene element during mouse-drag moves so hover feedback no longer jumps to other elements mid-drag.
- Fixed quick-click fixture selection in the 3D view so hovered fixtures can still be selected when the precise release pick misses.
- Fixed MVR fixture Color handling so visualization colors are no longer exported as gel/filter colors, while imported MVR Color values are preserved as fixture gel/filter data.
- Restored edge-safe fixture visibility and made fixture hover picking use actual fixture geometry so highlights and labels target the visible fixture instead of broad bounds.
- Fixed fixture ID edits so saved project files and exported MVR files keep the updated numeric fixture IDs instead of reverting to imported IDs.
- Fixed command-bar position and rotation value parsing so `t` and `thru` separators distribute selected items the same way as two space-separated values.
- Fixed MVR Eurotruss rendering in the 3D viewport by preserving native 3DS SceneObject mesh dimensions, keeping repeated SceneObject Symbol children distinct per parent object, and preserving correct truss fallback sizing for rotated trusses.
- Improved truss file validation so unsupported model formats are rejected clearly, while direct GLB and 3DS truss models now load only when the selected file exists.
- Improved fallback dimensions for direct truss model files so GLB and 3DS trusses remain visible and selectable even when model metadata or mesh loading is unavailable.
- Fixed Add scene object so reusing an existing Perastage basic geometry object copies its primitive geometry data immediately instead of showing a default cube until the project is reopened.
- Fixed basic geometry edit dialogs so position, screen size, and pipe length fields use the selected project distance units consistently.

## Stability and reliability

- Added local crash reporting and persistent diagnostics logs with build, platform, wxWidgets, OpenGL, recent-log, and stack-trace context when available.
- Improved diagnostics shutdown behavior so closing Perastage is not delayed by pending background log messages.
- Improved truss archive extraction safety by rejecting unsafe ZIP entries that use absolute paths or attempt to write outside the extraction cache.
- Improved 3D resource lookup resilience by bounding recursive fallback scans on the viewport sync path and logging when protected folders are skipped or scan limits are reached.
- Improved visible 3D resource synchronization so filesystem errors on cached or user-derived GDTF and model paths are handled safely with concise diagnostics instead of interrupting viewport updates.
- Improved 3D MVR import resilience so malformed model or GDTF resources no longer stop the rest of the scene from synchronizing into the viewport, while model texture loading avoids duplicate wxWidgets image-handler registration.
- Restored reliable 3D viewport updates after MVR imports by keeping external fixture-library paths absolute instead of rewriting them as scene-relative escape paths, and by preparing OpenGL before pending scene resource synchronization runs.
- Improved truss add-path handling so scene-local model resources are converted safely without filesystem exceptions escaping the UI.
- Improved 2D viewer startup reliability by validating GLEW initialization before enabling 2D OpenGL rendering.
- Improved 2D viewer interaction stability by safely skipping selection and hover picking when the OpenGL context is not available.
- Improved 3D viewer interaction stability by safely skipping picking and resource synchronization when the OpenGL context cannot be activated.
- Improved 3D viewer paint stability by skipping rendering and GL-dependent overlay work until OpenGL initialization completes successfully.

## Documentation

- Documented basic geometry primitive creation defaults and editing controls.
- Documented local diagnostics export and release symbol ZIP assets for troubleshooting and maintainer workflows.

## Internal changes

- Hardened the macOS installer workflow so restored vcpkg dependency caches are rebuilt when they contain stale Xcode SDK metadata, preventing old runner paths from breaking current macOS builds.
- Improved macOS installer build diagnostics by saving full compiler logs, showing focused error context on failure, and uploading related CMake and vcpkg logs for maintainers.
- Restored macOS installer build compatibility with newer Xcode toolchains by making GDTF metadata cache timestamp keys use an explicit portable duration representation.
- Updated UTF-8 filesystem path handling across the codebase and pinned the macOS installer workflow to an explicit current-generation runner for more predictable CI builds.
- Restored macOS installer build compatibility with Xcode 16.4 by avoiding deprecated filesystem path construction during GDTF model loading.
- Improved installer build reliability by allowing the Windows packaging workflow to use the current Visual Studio generator when available, generating Windows Release symbol files for CI symbol archives, using a dynamic modern Windows installer wizard style, and keeping macOS command-bar float parsing portable across libc++ versions.
- Improved GDTF loading efficiency by reusing cached archive content hashes when file metadata is unchanged, avoiding repeated full-file reads while preserving content-based cache invalidation.
- Improved rider truss import efficiency by caching truss definition loads within each import, avoiding repeated retries for the same valid or invalid truss paths while preserving existing parsed truss data precedence.
- Improved rider imports by reusing parsed GDTF fixture metadata during fixture creation and skipping repeated category inference once an imported fixture has an authoritative category, reducing repeated GDTF lookups while preserving existing dictionary, category, weight, and power precedence.
- Improved rider fixture filtering performance by reusing cleanup regular expressions and per-line section keyword normalization in hot preview paths while preserving existing filtering behavior.
- Improved rider fixture splitting performance during text import and fixture filter preview by avoiding intermediate string lists while preserving existing parsing behavior.
- Improved rider fixture filter preview construction by avoiding an extra string copy when appending rigging content, while preserving the existing preview layout.
- Improved rider text import internals by separating filtered rider requests into a parsed intermediate model, preserving preview output while reducing coupling between filtering and scene creation.
- Improved text-to-scene creation so applying the rider text filter in the editor no longer repeats the same filtering work during import, while preserving existing import behavior for all other flows.
- Improved Create from text import performance by loading fixture and truss dictionaries once per import, reusing importer-local lookup caches, and adding concise phase timing diagnostics for future optimization.
- Added backward-cpp integration, generated build metadata, and release workflow symbol archives for Windows, Linux, macOS, and Arch Linux builds.
- Improved truss table reload stability so rebuilding rows preserves existing truss selection without triggering transient selection side effects.
- Added concise diagnostics for truss loading and 2D viewer picking to make validation and interaction issues easier to troubleshoot.
- Improved grouped-hover rendering integration so symbol capture paths use the same highlight state as direct 3D drawing.
