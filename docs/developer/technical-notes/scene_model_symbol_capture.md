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

1. Copy the requested instance into an immutable capture-only scene snapshot.
2. Optionally align the instance transform to local axes.
3. Apply capture-focused config overrides (hide grid/labels, force symbol-friendly render settings).
4. Capture source RGBA images for `Front`, `Top`, `Side` (mirrored to `Left`), and inverted `Top` (as `Bottom`).
5. Convert images to vector symbols with `symbols::Symbol2DImageBuilder`.

The render data source is scoped to one synchronous offscreen update/render operation
on the GUI/render thread and is removed before control can return to the wx event loop.
Controller scene references are cleared before that scope ends. Each orthographic view
therefore has its own bounded scope, allowing a future cooperative pipeline to yield
between views without leaving capture data installed. Interactive viewers continue to
use the live scene; the bounded data-source override is thread-local, and no wx event
processing occurs inside it. Support/hoist rendering is disabled by the symbol-capture
render profile so unrelated supports cannot leak into the isolated image.
The service never swaps or clears live `ConfigManager` containers and never changes
capture color, transform, visibility, or selection on the live fixture. The snapshot
is discarded on both successful and failed capture, so the project scene is unchanged.

## Current integration

The existing fixture symbol generation tool now delegates capture to this service, so the same pipeline can be reused by future PDF/layout vector workflows.
