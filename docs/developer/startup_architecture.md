# Startup architecture

Perastage startup uses the splash lifetime as the ownership boundary for an
authoritative project restore. The application first initializes diagnostics,
preferences, localization and library paths, then constructs the lightweight
window shell. Platform and command-line open requests are resolved before the
project is committed to the visible UI.

## Restore phases

1. Resolve the explicit platform/command-line request or last-project fallback.
2. Traverse the project package and restore `scene.mvr`, configuration,
   resources and optional layout-cache entries.
3. Resolve the saved view preset and active project layout.
4. Lazily construct only the viewport required by the resolved preset.
5. Populate project tables and layers from the authoritative scene.
6. Apply the saved AUI perspective and activate the saved project layout.
7. Refresh visible summaries and rigging data and publish `InteractiveReady`.
8. Schedule non-essential fixture-symbol preparation.

The UI thread owns wxWidgets controls, AUI perspective commits, viewport
creation and OpenGL contexts. Background services may prepare immutable symbol
inputs after `InteractiveReady`; their existing project epoch checks prevent a
result from being published into a replacement project.

## InteractiveReady

`InteractiveReady` is emitted immediately before the startup splash is hidden.
At that point the authoritative scene and configuration are loaded, the saved
view and active layout are selected, required panes exist, visible project
panels have current data, and no startup callback remains that will replace the
layout. Deferred external opens remain queued and are processed after this
boundary.

## Project cache transfer

The primary `ProjectSession` ZIP traversal captures bounded entries below
`resources/layout_view_cache/` in a typed archive-resource payload. The layout
viewer clears stale state and consumes that payload. Cache parsing and deep
source validation remain optional and non-fatal; cache failure cannot affect
scene or configuration restore. The normal load path never reopens the `.pstg`
to acquire this cache.

## Profiling

Diagnostic logs contain stable `StartupProfile` records. The
`event=InteractiveReady` record reports elapsed milliseconds, layout commit and
activation counts, viewport ensure/construction counts, and project archive
open/traversal counts. A second record identifies post-startup fixture-symbol
scan scheduling. Project-load phase timings continue to be reported by the
layout render profiler when performance profiling is enabled.
