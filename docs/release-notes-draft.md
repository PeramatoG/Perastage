# Perastage v1.4.0 Release Notes

Changes since `v1.3.0`.

## Highlights

## New features

- Added MVR Import / Export preferences with a truss geometry export mode selector, defaulting to the standard MVR representation while offering a direct Geometry3D compatibility mode for truss symbols.
- Added a disabled-by-default Magnet snapping toolbar mode for 2D and 3D dragging, with truss-to-truss grouping on committed snaps and transform-only fixture/object snaps that preserve Hang Position and MVR compatibility.
- Added a project-persistent Axis Lock toolbar toggle for selection dragging, enabled by default for axis-constrained moves and allowing free 2D movement plus Blender-style view-plane movement in 3D when disabled.
- Expanded basic primitive creation and editing for spheres, cubes, and cylinders with editable names, project-persistent default dimensions, default load/save buttons, and richer edit controls for position and rotation.
- Added Edit menu Group and Ungroup commands with `Ctrl+G` and `Ctrl+U` shortcuts for cross-table scene selections, preserving object placement and hang-position assignments while using MVR-compatible GroupObject hierarchy.
- Expanded Add Truss with real-world X/Y/Z insertion coordinates in the active distance units, automatic multi-truss line placement, and default grouping into one bridge.
- Added Help menu actions to open the local logs folder and export a manual diagnostic report for troubleshooting.
- Added a startup update reminder checkbox so users can stop seeing repeated prompts for the same available version while keeping manual update checks available.

## Improvements

- Changed grouped fixture dragging so moving a selected fixture only moves that fixture, allowing fixtures to be repositioned along grouped trusses without moving the full group.
- Added a 2D position gizmo while dragging scene elements so movement direction feedback follows the dragged insertion point and matches the 3D viewer.
- Improved GroupObject handling so selecting a grouped member highlights and transforms the full root group, including nested groups, in 2D, 3D, and command-bar transform workflows.
- Improved group hover feedback by using a more yellow primary highlight and a paler secondary group highlight across the 3D view and related fixture, truss, hoist, and scene object table rows, matching the table selection styling.
- Improved 3D click selection for grouped scene items so a quick click selects the full group across fixture, truss, hoist, and scene object tables.
- Improved the 3D move gizmo with Blender-style cone arrowheads so drag axes are easier to distinguish from coordinate axes.
- Improved 2D and 3D drag feedback so the bottom X/Y/Z status readout shows the dragged insertion-point position in a highlighted color while objects are being moved.

## Fixes

- Preserved embedded MVR GDTF fixture references when reopening `.pstg` projects so fixtures affected by GDTF dictionary conflicts do not silently downgrade to dummy geometry on the next save.
- Improved MVR compatibility by preserving Support objects during roundtrip export, preserving geometry-less Support nodes with empty Geometries and using small 10 cm placeholder geometry for otherwise empty SceneObject exports, and writing Truss children in schema-compatible order.
- Fixed MVR Symbol/Symdef truss export so every Symbol keeps or receives a stable valid UUID and required Symdef reference, improving compatibility with strict MVR importers.
- Improved MVR 1.6 truss export compatibility by writing canonical Truss UUIDs, moving Perastage truss metadata to root UserData, and preserving legacy truss metadata import.

- Fixed GDTF persistence so automatic symbol generation and project-scoped fixture edits update project-owned copies or stable @Perastage library derivatives without filling the user fixture library with imported or generated project files.
- Fixed Linux GTK layout warnings in the rich text toolbar by giving icon buttons enough themed padding while preserving the existing editing behavior.
- Fixed Magnet fixture-to-truss snapping so fixtures snap to the actual truss edge height, stay aligned when their snapped truss or full snapped group is moved, and only snap after an intentional drag starts.
- Improved Magnet truss snapping so grouped truss runs can snap to loose trusses using the truss run bounds while ignoring attached fixtures.
- Fixed reopening the last project from startup and loading its symbol cache metadata when the saved path contains accented or other non-ASCII characters.

- Improved Magnet snapping so fixtures and trusses release from snapped targets based on the raw mouse-following drag position, fixture-to-truss snaps join the fixture to the truss snap group, and detaching snapped fixtures or trusses preserves the rest of the group hierarchy.
- Fixed highlighted drag-coordinate feedback so the bottom X/Y/Z readout stays visible and reliably changes font color during 2D and 3D object moves.
- Fixed scene object renaming so edited Data View names are preserved in the scene summary and MVR exports, supporting technical object-name workflows such as cable waypoints.
- Kept the 2D and 3D viewer highlights pinned to the dragged scene element during mouse-drag moves so hover feedback no longer jumps to other elements mid-drag.
- Fixed quick-click fixture selection in the 3D view so hovered fixtures can still be selected when the precise release pick misses.
- Fixed MVR fixture Color handling so visualization colors are no longer exported as gel/filter colors, while imported MVR Color values are preserved as fixture gel/filter data.
- Fixed MVR layer color persistence so Perastage now stores layer appearance metadata in a standards-compliant root UserData block, while still importing legacy Layer/Color data from older exports.
- Fixed MVR fixture UnitNumber export so existing unit numbers are preserved and missing values are generated deterministically by fixture type and stage position.
- Restored edge-safe fixture visibility and made fixture hover picking use actual fixture geometry so highlights and labels target the visible fixture instead of broad bounds.
- Fixed fixture ID edits so saved project files and exported MVR files keep the updated numeric fixture IDs instead of reverting to imported IDs.
- Fixed command-bar position and rotation value parsing so `t` and `thru` separators distribute selected items the same way as two space-separated values.
- Fixed MVR Eurotruss rendering in the 3D viewport by preserving native 3DS SceneObject mesh dimensions, keeping repeated SceneObject Symbol children distinct per parent object, and preserving correct truss fallback sizing for rotated trusses.
- Improved truss file validation so unsupported model formats are rejected clearly, while direct GLB and 3DS truss models now load only when the selected file exists.
- Improved fallback dimensions for direct truss model files so GLB and 3DS trusses remain visible and selectable even when model metadata or mesh loading is unavailable.
- Fixed Add scene object so reusing an existing Perastage basic geometry object copies its primitive geometry data immediately instead of showing a default cube until the project is reopened.
- Fixed basic geometry edit dialogs so position, screen size, and pipe length fields use the selected project distance units consistently.
- Fixed 3D truss height labels so they show both the value and suffix in the selected project distance units.

## Stability and reliability

- Fixed Linux Unicode text stability by initializing a UTF-8 process text locale at startup and removing narrow-string wxWidgets formatting from affected status, splash, layout-progress, and viewport-label paths.
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

- Added Linux text-locale startup validation coverage to prevent regressions in wxWidgets narrow-to-wide string conversion behavior.
- Improved Debian/Linux test build reliability by linking logger- and config-service-based test executables with the shared diagnostics, app path, filesystem path, config service, and symbol-cache support sources used by the main application.
- Improved Linux GCC build reliability by separating lightweight GDTF geometry data types from loader declarations, reducing header-order coupling in the 3D resource and bounds systems.
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
