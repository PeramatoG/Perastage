# Viewer2D state ownership

Viewer2D state is intentionally classified before deeper Layout/offscreen refactors. This document records the current ownership model and the intended future separation between runtime state, user preferences/config state, and project/Layout persistent state. The goal is to let future work separate these paths without changing rendering output, project files, Layout JSON, PDF export, print preview, or printing behavior.

## Current problem

The current Viewer2D state flow mixes several responsibilities through shared code paths:

- live panel/runtime state, such as the active camera while a user pans or zooms;
- user preferences stored in `ConfigManager`, such as default grid, ruler, label, and dark-mode settings;
- project/Layout persistent state stored in embedded Layout 2D view definitions;
- temporary Layout preview/capture state used to render embedded 2D views through the existing Viewer2D pipeline.

`viewer2d::Viewer2DState`, `ConfigManager`, `viewer2d::ScopedViewer2DState`, Layout preview capture, Layout rasterization, and editor preferences currently overlap. Some values are temporary runtime/capture values, some should survive between projects as user preferences, and some belong to persistent project/Layout definitions. This document makes those boundaries explicit for future refactors without changing current behavior.

## Current state structures

- `viewer2d::Viewer2DState` is the current bridge snapshot used by both editor capture and embedded Layout 2D view rendering. It groups camera, render-option, and layer state even though those fields do not all have the same long-term owner.
- `layouts::Layout2DViewCameraState` stores the camera/view fields used by an embedded Layout 2D view, including offsets, zoom, viewport dimensions, and selected view.
- `layouts::Layout2DViewRenderOptions` stores rendering choices used by the Viewer2D path, including render mode, dark mode, grid, ruler, labels, and top-fixture inversion. Some of these values are user preferences in the editor and project/Layout state when embedded in a Layout definition.
- `layouts::Layout2DViewLayers` stores hidden layers and hidden fixture types for a Layout 2D view. This is transitional because the editor also exposes similar visibility through `ConfigManager`.
- `layouts::Layout2DViewDefinition` is the project/Layout persistent definition for an embedded Layout 2D view. It owns the frame, camera, render options, layer visibility, draw-frame setting, z-index, and id for that Layout element.
- `viewer2d::CaptureState(...)` reads the live panel camera when a panel is available, falls back to camera values in `ConfigManager` otherwise, and reads render/layer options from `ConfigManager`.
- `viewer2d::ApplyState(...)` currently writes the supplied state into `ConfigManager` and optionally refreshes the Viewer2D panels. When `persistCameraToConfig` is false, the camera is applied to the panel without committing camera values to config.
- `viewer2d::ScopedViewer2DState` is the scoped temporary preview/capture mechanism. It captures the previous Viewer2D state, applies a replacement state, and restores the previous state on scope exit.
- `viewer2d::FromLayoutDefinition(...)` converts project/Layout persistent state from a `Layout2DViewDefinition` into the bridge `Viewer2DState` used by the current render path.
- `viewer2d::ToLayoutDefinition(...)` converts a `Viewer2DState` snapshot into project/Layout persistent state, including the supplied Layout frame.
- `viewer2d::CaptureLayoutDefinition(...)` captures current editor state and converts it into a `Layout2DViewDefinition` when the user is creating or updating an embedded Layout 2D view.

## Ownership classification

| Ownership category | Current fields and examples | Current persistence expectation |
| --- | --- | --- |
| Runtime state | Active live panel camera while the user pans/zooms, transient viewport dimensions, temporary capture framebuffer override, temporary offscreen capture state, repaint/render scheduling flags, capture-in-progress flags, command-buffer capture metadata, render overrides used only for capture, hover/drag/selection overlay state | Runtime-only values must not be serialized into Layout definitions or saved to user configuration unless an explicit editor action owns that persistence. |
| User preferences/config state | Default Viewer2D dark mode, grid visibility/style/color, ruler visibility/color, label display preferences, label font sizes and offsets, fixture label defaults, render mode defaults, top-fixture inversion defaults, general editor preferences that should survive between projects | User preferences/config state belongs to `ConfigManager` and should survive application sessions, but should not be overwritten by project restore unless the value is explicitly project-owned. |
| Project/Layout persistent state | `Layout2DViewDefinition::frame`, stored camera for an embedded Layout 2D view, render options stored in a Layout definition, hidden layers or hidden fixture types stored as part of a Layout 2D view, draw-frame and z-index information, the Layout 2D view id | Project/Layout persistent state belongs to the project/Layout model and may be serialized only through the existing Layout/project save paths. |
| Ambiguous/transitional state | Hidden layers currently exposed through `ConfigManager`, `view2d_dark_mode` currently preserved during project load as a user preference, `forceBottomViewForTopFixtures` which may come from config or a Layout definition depending on context | Future refactors should resolve these one at a time with compatibility notes and should not change project file or Layout JSON formats accidentally. |

## Rules for future refactors

- Temporary Layout preview capture must not permanently mutate user Viewer2D preferences.
- Applying an embedded Layout 2D view for preview must be scoped and restored through `viewer2d::ScopedViewer2DState` or an equivalent scoped temporary Layout preview/capture path.
- Project load must not leak the previous project's Layout preview cache, command-buffer cache, persistent RGBA cache, texture state, PBO state, or failure diagnostic state.
- PDF/export/print must not depend on Layout preview texture, PBO, persistent RGBA cache, or failure diagnostic state.
- Runtime-only fields must not be serialized into Layout definitions unless explicitly intended and documented as project/Layout persistent state.
- User preferences should not be overwritten by project restore unless the value is explicitly project-owned.
- Future FBO/offscreen cleanup should stay separate from state ownership cleanup. Replacing or reducing the hidden wx offscreen host is a rendering-target task, not a reason to merge runtime, config, and project/Layout ownership.
