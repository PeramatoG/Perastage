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
  - `true`: temporarily removes world rotation while preserving per-axis scale, so orthographic views are captured in the object's local axes (useful for rotated instances).
- `options.forcedFixtureColor`: optional color override for fixture captures.

### Output

`SceneModelSymbolCaptureResult` returns:

- `ok`: true when all four views were captured and converted.
- `error`: user-readable error when capture fails.
- `symbols`: generated `symbols::Symbol2D` list.

## Internal process summary

1. Isolate the requested scene instance in a temporary scene override.
2. Optionally align the instance transform to local axes.
3. Apply capture-focused config overrides (hide grid/labels, force symbol-friendly render settings).
4. Capture source RGBA images for `Front`, `Top`, `Side` (mirrored to `Left`), and inverted `Top` (as `Bottom`).
5. Convert images to vector symbols with `symbols::Symbol2DImageBuilder`.

## Current integration

The existing fixture symbol generation tool now delegates capture to this service, so the same pipeline can be reused by future PDF/layout vector workflows.
