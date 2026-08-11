# Scene model symbol capture service

This document describes the reusable API that captures vector symbols for any 3D model instance present in the scene.

## Purpose

`CaptureSceneModelOrthographicSymbols` generalizes the workflow used by the fixture symbol generation tool so other modules can request symbols for:

- Fixture instances.
- Truss instances.
- Generic scene objects.

The result is a vector-ready set of orthographic symbols (`Front`, `Top`, `Left`, `Bottom`) generated from the same 2D offscreen capture pipeline.

## Location

- Header: `gui/tools/scene_model_symbol_capture_service.h`
- Implementation: `gui/tools/scene_model_symbol_capture_service.cpp`

## Public API

```cpp
SceneModelSymbolCaptureResult
CaptureSceneModelOrthographicSymbols(Viewer2DOffscreenRenderer &renderer,
                                     ConfigManager &cfg,
                                     const SceneModelSymbolTarget &target,
                                     const SceneModelSymbolCaptureOptions &options = {});
```

### Input

- `target.kind`: `Fixture`, `Truss`, or `SceneObject`.
- `target.uuid`: UUID of the specific scene instance.
- `options.alignToLocalAxes`:
  - `false` (default): captures with current world orientation.
  - `true`: removes world rotation on the capture copy while preserving per-axis scale, so orthographic views are captured in the object's local axes (useful for rotated instances).
- `options.forcedFixtureColor`: optional color applied only to the capture copy.

### Output

`SceneModelSymbolCaptureResult` returns:

- `ok`: true when all four views were captured and converted.
- `error`: user-readable error when capture fails.
- `symbols`: generated `symbols::Symbol2D` list.

## Internal process summary

1. Install a cloned requested instance in the private compatibility scene.
2. Optionally align the instance transform to local axes.
3. Apply geometry-neutral extraction overrides (white background, hidden grid,
   ruler, and labels, plus the symbol extraction color and line style).
4. Capture canonical source RGBA images through the corresponding
   `Viewer2DPanel` cameras: `Front`, `Top`, `Side` (stored as `Left`), and
   `Bottom`. The reference raster is not mirrored or otherwise transformed.
5. Convert images to vector symbols with `symbols::Symbol2DImageBuilder`.

For a fixture whose instance/root rotation is normalized for reuse, every
pre-vectorization image uses the same camera, projection, exact GDTF mode,
child transforms, mesh processing, depth behavior, and part inclusion as the
equivalent normal 2D view. Fixture bounds include every rendered mesh,
including lens geometry, and are resolved from that same exact mode. The only
intentional viewer differences are the geometry-neutral extraction overrides
listed above.

The render data source is scoped to one synchronous offscreen operation on the
GUI/render thread and remains installed for warm-up plus all four orthographic views.
The narrow compatibility boundary temporarily swaps the renderable `ConfigManager`
containers so every legacy and current renderer query sees the same cloned target.
No wx event processing occurs inside it. Strict RAII restores fixtures, trusses, scene
objects, and supports once on success or failure; selection, layers, project dirty
state, and persistent data are not changed. This behavior is private to deterministic
symbol capture and must not be reused as a general scene-management mechanism.

## Current integration

The existing fixture symbol generation tool now delegates capture to this service, so the same pipeline can be reused by future PDF/layout vector workflows.
