# Perastage v1.4.0 Release Notes

Changes since `v1.3.0`.

## Highlights

## New features

- Added a startup update reminder checkbox so users can stop seeing repeated prompts for the same available version while keeping manual update checks available.

## Improvements

## Fixes

- Improved truss file validation so unsupported model formats are rejected clearly, while direct GLB and 3DS truss models now load only when the selected file exists.
- Improved fallback dimensions for direct truss model files so GLB and 3DS trusses remain visible and selectable even when model metadata or mesh loading is unavailable.

## Stability and reliability

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

## Internal changes

- Improved truss table reload stability so rebuilding rows preserves existing truss selection without triggering transient selection side effects.
- Added concise diagnostics for truss loading and 2D viewer picking to make validation and interaction issues easier to troubleshoot.
