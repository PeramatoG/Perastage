# Startup architecture

Perastage creates an application-owned `startup::Metrics` context at the start
of `OnInit`. The context follows configuration setup, window construction,
startup-path resolution, project loading, and the final UI commit. The splash
lifetime is the ownership boundary for an authoritative project restore.

## Restore phases

1. Resolve the explicit platform/command-line request or last-project fallback.
2. Traverse the project package and restore `scene.mvr`, configuration,
   resources and optional layout-cache entries.
3. Resolve the saved view preset and active project layout.
4. Resolve the semantic viewport requirement from the saved view mode and
   lazily construct that viewport before loading the AUI perspective. Legacy
   perspectives are inspected only when no recognized semantic mode exists.
5. Populate project tables and layers from the authoritative scene.
6. Apply the saved AUI perspective and activate the saved project layout.
7. Refresh visible summaries and rigging data and publish `InteractiveReady`.
8. Schedule non-essential fixture-symbol preparation.

The UI thread owns wxWidgets controls, AUI perspective commits, viewport
creation and OpenGL contexts. Background services may prepare immutable symbol
inputs after `InteractiveReady`; their existing project epoch checks prevent a
result from being published into a replacement project.

## InteractiveReady

`InteractiveReady` is emitted once from the idle-driven splash completion pass,
immediately before the startup splash is hidden.
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

Diagnostic logs and the internal CMD panel receive the stable
`StartupProfile event=InteractiveReady` summary. It reports measured bootstrap
phase durations; project archive open, traversal, scene extraction, cache
transfer and MVR restore values; layout and AUI commits; viewport ensures and
constructions; and authoritative table/layer reloads. A second diagnostic-log
record identifies post-startup fixture-symbol scan scheduling. This boundary
does not claim to measure the first completed paint.

Reusable project panels use an explicit initial-population policy. MainWindow
requests deferred population so startup data is built once after project load;
standalone construction defaults to immediate population. Layout2DViewDialog
keeps its existing first-show layer reload and therefore also requests deferred
construction.
