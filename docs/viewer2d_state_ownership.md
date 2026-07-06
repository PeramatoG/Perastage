# Viewer2D state ownership

Viewer2D state is intentionally classified before deeper Layout/offscreen refactors. This document keeps runtime interaction state, user preference/config state, and project/Layout definition state separate so temporary Layout preview rendering does not accidentally become persistent application or project state.

## Ownership categories

| Category | Owner | Examples | Persistence rule |
| --- | --- | --- | --- |
| Runtime-only state | Active Viewer2D panels, render panels, and temporary capture guards | Current interactive panel offsets, zoom, viewport size, view while the user is navigating, temporary capture state, dragging, hover, measurement, selection overlays, repaint throttling, capture-in-progress flags, render overrides, and offscreen capture-panel setup | Must not be persisted just because a Layout preview needs a temporary render. Temporary preview capture must restore the previous state through `viewer2d::ScopedViewer2DState` or an equivalent scoped mechanism. |
| User preference/config state | `ConfigManager` and intentional editor preference actions | Default 2D render mode, dark mode, grid visibility, grid style, grid color, grid draw order, ruler visibility and colors, label visibility, label font sizes, label offsets, top-fixtures inversion, hidden layers, hidden fixture types, and the persisted editor camera when the user intentionally changes the editor view | May survive application sessions only when changed through normal editor preference or view flows. Layout preview capture/rasterization must not call `SaveUserConfig` or introduce new config persistence. |
| Project/Layout definition state | `layouts::Layout2DViewDefinition` stored in the project/Layout model | Embedded Layout 2D view frame, camera, render options, layers, drawFrame, zIndex, and id | Belongs to the project/Layout model. Runtime navigation and temporary capture state must not mutate or save project/Layout definitions unless the user is editing the Layout definition itself. |

## Current Viewer2D state bridge

`viewer2d::Viewer2DState` is a bridge structure used by both editor state capture and Layout 2D view rendering. It currently contains:

- `layouts::Layout2DViewCameraState` for camera/view values.
- `layouts::Layout2DViewRenderOptions` for render preferences and embedded view render options.
- `layouts::Layout2DViewLayers` for hidden layers and hidden fixture types.

`viewer2d::CaptureState(...)` currently reads camera values from a `Viewer2DPanel` when one is available, and reads render options, labels, grid, ruler, and layer visibility from `ConfigManager`. `viewer2d::ApplyState(...)` currently writes many of those values back into `ConfigManager` because the editor still uses shared config-backed Viewer2D state. This behavior is documented here as existing behavior, not as a target architecture.

The lightweight ownership map in `viewer2d/viewer2dstate.h` and `viewer2d/viewer2dstate.cpp` identifies known Viewer2D config keys as user preference/config state. Future refactors should use that map as a checklist when moving runtime-only state away from shared configuration.

## Layout preview capture and rasterization rules

Interactive Layout preview 2D views may temporarily apply a `layouts::Layout2DViewDefinition` to the capture panel so the existing Viewer2D render path can produce command buffers or RGBA pixels. That temporary application is runtime-only capture state.

Layout preview capture/rasterization code must:

- Use `viewer2d::ScopedViewer2DState` or an equivalent scoped restore mechanism when applying temporary 2D view state.
- Pass `persistCameraToConfig = false` or an equivalent non-persistent mode for temporary preview captures.
- Avoid direct `SaveUserConfig`, project save, or Layout definition save calls.
- Leave `LayoutViewerPanel` as the owner of interactive preview caches, OpenGL texture ids, PBO state, persistent RGBA cache, failure diagnostics, frame drawing, selection handles, and preview placeholders.
- Keep PDF/export/print separate from the interactive preview texture path and preview placeholder/failure diagnostic state.

## Future FBO/offscreen cleanup note

The current `Viewer2DOffscreenRenderer` still uses a hidden/off-screen wx window and `Viewer2DPanel` for capture. Replacing or reducing that dependency with a more explicit framebuffer/render-target capture path is a separate future task. The FBO/offscreen cleanup should not be mixed with Viewer2D state ownership changes; state boundaries should be clarified first, then the rendering target can be refactored with less risk.
