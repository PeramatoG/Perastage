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
8. Maximize and show MainWindow with its authoritative layout under an update
   lock. Queue a continuation for show/maximize sizing and shell publication,
   commit final sizes, and publish native child controls once.
   For a visible 3D view, retain the splash until all source resources have
   completed and a natural scene paint has presented them with input enabled.
   Only then emit `InteractiveReady` and dismiss the splash; optional navigation
   representations may continue preparing.
9. Schedule non-essential fixture-symbol preparation.

The UI thread owns wxWidgets controls, AUI perspective commits, viewport
creation and OpenGL contexts. Background services may prepare immutable symbol
inputs after `InteractiveReady`; their existing project epoch checks prevent a
result from being published into a replacement project.

## InteractiveReady

`InteractiveReady` is emitted once from the queued splash completion pass.
The initial idle pass shows the previously hidden MainWindow under an update
lock. It queues a `CallAfter` continuation to resolve final native sizes, commit
AUI/layout, and explicitly refresh and update native control subtrees to
establish the native shell boundary. For visible 3D startup, an observer of normal viewer
paint events queues a readiness check after the renderer has returned. Splash
dismissal requires source loading to finish, a scene presentation, an enabled
frame/viewer and no viewport interaction lock. It does not wait for optional
navigation representations. Completion does not require another idle event: ongoing
progressive viewport activity can delay idle delivery after the frame becomes
visible. Showing the frame alone is not treated as completion of native painting.
The readiness check uses the read-only `HasPendingSourceResourceLoading()` query,
including resource synchronization not yet initialized, rather than status text
or a progress percentage. It never requests a scene paint or synchronizes resources.
The queued observer also records a scene presentation preceding shell publication,
so empty projects do not require another paint just to close the splash.
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

The version 6 Layout cache is raster-first. A matching bounded RGBA snapshot is
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

Version 6 validation binds cache metadata to the exact serialized `scene.mvr`
bytes and the sorted names and exact bytes of packaged Layout images. Both use
the `fnv1a64-v1` contract. Save computes these identities from buffers already
owned by the export/resource pipeline; load updates them while the primary ZIP
traversal reads either the bounded scene buffer or spill file. A fully covered
match takes the fast path and never calls the extracted-source deep hasher.
Schema 5 and incomplete-coverage caches retain conservative deep validation.

Legend persistence hashes the logical GDTF specification, mode, fixture type,
visible row settings and presentation values. Runtime symbol lookup continues
to use the resolved workspace path, but temporary `mvr-import-*` paths never
enter the persistent legend identity. Raster cache metadata is scoped to a
versioned Windows, macOS or Linux wxWidgets rendering contract so disposable
pixels are not reused across incompatible font/rendering environments.

The first active Layout rebuild uses a minimal event-loop yield rather than the
180 ms interactive-edit debounce. Queue wait and event-loop overrun are measured
from the actual request and dispatch boundaries. Warm raster painting no longer
constructs the offscreen Viewer2D renderer; capture resources are acquired only
when the requested view cannot be served by its exact persistent raster.

For Layout-mode startup, automatic fixture-symbol inspection is released by the
Layout render-ready event rather than immediately after `InteractiveReady`.
Candidate inspection then advances one fixture per idle slice, keeping paint,
timer and input events ahead of non-essential post-startup scanning.

## Project archive sources

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
measures the selected-view presentation boundary, not compositor completion or
completion of optional progressive navigation/quality representations.

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

## GUI publication during File → Open

Archive parsing, project restoration, and resource processing retain their
existing paths. After a successful archive restore, a scoped
`wxWindowUpdateLocker` holds the visible frame through saved-layout selection,
table/layer reloads and summary/rigging refreshes. The separate modal progress
dialog remains live; progress updates can dispatch events without publishing
partially rebuilt frame controls. Its auto-hide terminal value is reserved for
the final GUI publication call, because archive sub-stages can each report a
completed range before GUI composition begins. Status messages continue to be
recorded.
An archive-load failure occurs before this lock; an exception or early exit
inside the composition scope automatically thaws the frame.

Saved-layout application configures newly required viewport panes, perspective,
visibility, docking and minimum sizes before its AUI commit. Interactive
viewport construction and manual preset commands retain their own commits.
Summary captions defer their AUI commit while the frame is frozen; the final
project-load AUI/layout/size commit includes them.

At the publication boundary the frame thaws and native GUI subtrees are
refreshed/updated once before completing the progress dialog's auto-hide range
and releasing the load disabler. `PublishNativeGuiChildren` excludes GL canvases
and any ancestor containing one, including Inspector preview canvases.
Before exposing controls, native erase backgrounds are initialized from the
active system theme: content controls use the window colour and shell controls
use the button-face colour. Explicit colours (such as Console's black
background) are preserved. Initialization also excludes GL-containing subtrees
and is applied to initially hidden native controls so that later activation has
the same background. It does not change scene clear colours or render resources.
MSW
`Update()` only paints the target HWND, so wx child controls are also updated.
GTK can recursively refresh/update native children, which is why simply
calling `MainWindow::Update()` would be unsafe for this purpose. No platform
specific branch, new yield, timer, sleep, or resource-completion threshold is
used. Shell painting, structural readiness, and progressive scene completion
remain separate boundaries. The Viewer3D 12 ms work budget, 128-resource batch
limit and 150 ms presentation cadence are unchanged.

## Manual visual regression procedure

Use STG MAHOU or another very large project on Windows, Linux and macOS. Save
and repeat each scenario with 2D, 3D, Layout Mode and Inspector presets:

1. Restore the project as the last project at startup. Confirm the splash stays
   visible through composition and the interface appears in its final docking
   positions with populated fixture, truss, layer, summary and rigging controls.
2. Open the project through File → Open while another project is already open.
   Confirm progress continues and no empty/white controls or intermediate pane
   positions are exposed during reconstruction.
3. Cancel the file chooser, then attempt to open an invalid project archive.
   Confirm the frame remains enabled, repainting and usable after failure.
4. With 3D visible, confirm optional representations can continue preparing after
   the startup splash disappears. File → Open may also keep loading source
   resources after its project dialog disappears; status reporting remains live.
   For 3D startup specifically, the splash must remain through “Loading scene: …”
   and may close at “Preparing scene for navigation: …” after source geometry has
   been presented. Check orbit, pan, wheel zoom and keyboard camera actions after
   closure. The visible viewer must be enabled, interaction unlocked, and keyboard
   focus restored to it. Optional representations use the original source mesh
   until ready; dense scenes may navigate less smoothly until proxies finish.
   A populated shell must not retain the splash solely because optional navigation
   representations are still being prepared.

Native shell publication excludes the viewports. Startup separately waits for
source-resource completion and a natural scene paint, but not for optional
navigation/quality preparation. Neither boundary synchronizes resources or
requests extra viewport frames. Where
diagnostics are available, compare the same project and preset before/after for
resource synchronization count, scene paint count and load duration. Separate
the one-time native control publication cost from ongoing scene rendering.
