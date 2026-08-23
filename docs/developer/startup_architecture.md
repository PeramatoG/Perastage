# Startup architecture

Perastage creates an application-owned `startup::Metrics` context at the start
of `OnInit`. The context follows configuration setup, window construction,
startup-path resolution, project loading, and the final UI commit. MainWindow
exists as the application top window but remains hidden while the splash owns
the visible startup experience.

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
7. Refresh visible summaries and rigging data and perform the final AUI update.
8. Maximize and publish MainWindow with its authoritative layout, emit
   `InteractiveReady`, and then dismiss the splash.
9. Schedule non-essential fixture-symbol preparation.

The UI thread owns wxWidgets controls, AUI perspective commits, viewport
creation and OpenGL contexts. Background services may prepare immutable symbol
inputs after `InteractiveReady`; their existing project epoch checks prevent a
result from being published into a replacement project.

## InteractiveReady

`InteractiveReady` is emitted once from the idle-driven splash completion pass.
That pass performs the final AUI update and publishes the previously hidden
MainWindow before dismissing the splash, so the structural setup layout is
never exposed as an intermediate application frame.
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

The version 5 Layout cache is raster-first. A matching bounded RGBA snapshot is
independent of the optional command-replay payload, so a view whose replay
graph exceeds 75,000 commands still saves its final raster. Command counting
stops as soon as the limit is exceeded. Individual raster resources are capped
at 8 MiB and the transferred Layout raster set is capped at 16 MiB; malformed
dimensions and byte counts are rejected without affecting authoritative
project loading.

The startup cache also retains the final legend raster. Hydrated view and
legend snapshots upload directly to the Layout Viewer context without requiring
the scene capture panel. Legacy schema versions are non-authoritative and are
safely rebuilt. When a cold render needs legend symbols, the captured snapshot
is published to every matching legend cache before the incremental renderer can
return after another element, preventing duplicate capture on the next tick.

## Phase 2 project archive sources

The primary `.pstg` traversal now captures `config.json` into an owned byte
payload and patches packaged layout-image paths before the configuration store
applies it. The normal path therefore performs one JSON parse and does not
create a temporary configuration file. Standalone JSON loading remains a thin
file wrapper over the same byte parser.

`scene.mvr` uses a bounded archive-source policy. Entries up to 128 MiB are
read into one owned buffer and passed to `MvrImporter`; larger entries spill to
the existing project temporary directory as they are read. The limit is named
at the `ProjectSession` API and can be reduced by deterministic tests. It is
conservative enough for normal projects while preventing archive metadata from
causing an unbounded allocation. The importer wraps either the file or memory
source in the same `wxInputStream` extraction implementation, including the
same traversal and case-collision checks. It continues to extract the inner
MVR workspace because GDTF and model consumers still require stable paths.

The startup record distinguishes memory restores, spill fallbacks, spill bytes,
configuration memory loads, and configuration patch time. Normal bounded
projects have one `.pstg` traversal, one scene-buffer restore, no project-level
scene/config temporary write, and retain only the required inner MVR resource
workspace. Spill files remain solely for scenes exceeding the memory policy.

## Deferred GDTF fingerprint validation

Automatic fixture-symbol preparation no longer invalidates semantic
fingerprints at read sites. Capture records both the memoized semantic
fingerprint and the cheap path/size/modification-time revision. Publication
reuses the captured fingerprint when both revisions are available and equal;
otherwise it performs the conservative semantic revalidation and rejects stale
work as before. GDTF writers remain responsible for invalidating or publishing
the cache at their existing mutation boundaries.

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

## Standard view modes

Saved project restore and manual standard-view selection are separate flows. A
saved perspective retains compatible project-specific docking and panel
visibility, while the required central pane and all five main toolbars are
enforced. The manual 3D, 2D, and Layout commands use explicit pane visibility
and docking recipes rather than perspectives captured from mutable runtime
state. This makes transitions history-independent even when a heavyweight
viewport is constructed for the first time after Layout Mode.
