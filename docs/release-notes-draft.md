# Perastage v1.4.0 Release Notes

Changes since `v1.3.0`.

## Highlights

## New features

- Added Edit menu Group and Ungroup commands with `Ctrl+G` and `Ctrl+U` shortcuts for cross-table scene selections, preserving object placement and hang-position assignments while using MVR-compatible GroupObject hierarchy.
- Added Help menu actions to open the local logs folder and export a manual diagnostic report for troubleshooting.
- Added a startup update reminder checkbox so users can stop seeing repeated prompts for the same available version while keeping manual update checks available.

## Improvements

- Improved GroupObject handling so selecting a grouped member highlights and transforms the full root group, including nested groups, in 2D, 3D, and command-bar transform workflows.

## Fixes

- Fixed 3D fixture hover feedback so highlight and labels repaint in the same frame after the hovered fixture changes, while keeping edge fixtures visible.
- Fixed fixture ID edits so saved project files and exported MVR files keep the updated numeric fixture IDs instead of reverting to imported IDs.
- Fixed command-bar position and rotation value parsing so `t` and `thru` separators distribute selected items the same way as two space-separated values.
- Fixed MVR Eurotruss rendering in the 3D viewport by preserving native 3DS SceneObject mesh dimensions, keeping repeated SceneObject Symbol children distinct per parent object, and preserving correct truss fallback sizing for rotated trusses.
- Improved truss file validation so unsupported model formats are rejected clearly, while direct GLB and 3DS truss models now load only when the selected file exists.
- Improved fallback dimensions for direct truss model files so GLB and 3DS trusses remain visible and selectable even when model metadata or mesh loading is unavailable.

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

- Documented local diagnostics export and release symbol ZIP assets for troubleshooting and maintainer workflows.

## Internal changes

- Added backward-cpp integration, generated build metadata, and release workflow symbol archives for Windows, Linux, macOS, and Arch Linux builds.
- Improved truss table reload stability so rebuilding rows preserves existing truss selection without triggering transient selection side effects.
- Added concise diagnostics for truss loading and 2D viewer picking to make validation and interaction issues easier to troubleshoot.
