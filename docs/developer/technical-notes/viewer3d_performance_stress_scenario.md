# Viewer3D stress scenario (baseline telemetry)

This scenario is the baseline used before architecture/performance changes in
Viewer3D. It is intentionally simple and reproducible so telemetry deltas can
be compared between PRs.

## Goal

Capture a stable telemetry window with:

- a **heavy geometry load** (many polygons from many fixtures), and
- one **lightweight isolated object** (small scene object far from the dense
  fixture cluster) to keep hover/highlight transitions measurable.

## Input assets

- Rider file: `tests/data/rider_large.txt`.
- One lightweight object created in-scene (recommended primitive: cube).

## Repro steps

1. Start the app with a clean session.
2. Import `tests/data/rider_large.txt`.
3. Open **Viewer3D** and wait until resource sync is idle.
4. Add one small scene object (cube) and place it far from the fixture block
   (for example +20 m on X).
5. Move the camera so both zones are reachable with short mouse moves:
   - dense fixture region (heavy polygon load),
   - isolated cube (light object).
6. Perform 20-30 seconds of interaction:
   - orbit/pan/zoom around dense fixtures,
   - move cursor across fixtures (trigger hover),
   - move cursor to isolated cube and back (trigger highlight changes).
7. Collect `Viewer3DPanel` debug lines for at least 10 one-second windows.

## Telemetry line to compare

`Viewer3DPanel refreshes/s full=... highlight=... full_render_ms=... hover_query_ms=... highlight_update_ms=...`

Use medians across captured windows when comparing branches/hardware sessions.

## Progressive resource and runtime detail contract

Interactive 3D and 2D viewports process at most eight unique resource requests
per synchronization pass, with an eight-millisecond cooperative deadline checked
between resources. One loader/upload remains indivisible, and scene traversal,
bounds and drawing are measured separately. Fixture requests precede trusses and
SceneObjects. Layer/type visibility controls eligibility; precise frustum bounds
are not required to load a resource. Captures retain synchronous complete loading.

The caller advances a generation on scene/resource/visibility mutations; bounded
continuations retain it. Work state contains owned string requests rather than scene pointers. A
request queue is built once per generation and resumes at its cursor; subsequent
batches do not renormalize every object reference. The viewport binds the coalesced custom refresh event directly and requests
continuation while resource work remains. Scene replacement cancels pending
representation references; explicit
asset replacement invalidates resolved object parts before GPU release. Repeated
references are processed once per generation. Failed resources retry after
invalidation rather than keeping an infinite background redraw loop.

Core exposes a monotonic render revision separate from dirty/saved revisions.
Touch, reset and rollback advance it; marking saved does not. Existing edit,
Command/CLI/OSC, Undo/Redo and import publication paths use that central state,
with viewer UpdateScene/resource-rebind notifications as explicit invalidation.
Lightweight updates compare revisions instead of sorting per-object strings.
The existing resource-content signature is retained on mutation, and reused
between progressive passes. SceneObject draw/pick resolution is reused between
resource/scene synchronizations. Local bounds survive additions and are discarded
when existing assets are released, avoiding repeated vertex scans during filling.

Set `PERASTAGE_PERF_DIAGNOSTICS=1` to emit aggregate operation counts/durations at
resource completion and the first presented resolved mesh frame. Counters include
extraction, parsing/application, sync, cache hits/misses/read/write, 3DS loads,
base processing, runtime representations, GPU uploads, bounds, visible sets,
frames/navigation, references and cache-root preparation. First-visible latency
starts at the first resource sync, rather than before file selection/import.
Separate the extraction/parse/application counters when assessing full latency.
Detailed existing debug telemetry remains useful for frame/navigation timing.

Deterministic regression tests use 10,000 SceneObjects and 4,490 mock resources,
assert eight-resource batches and eventual completion, cancel same-base scene
replacement, exercise hidden-only resources and physical invalidation, and verify
stable package cache reuse with changing extraction paths and companion textures.
`ModelDetail` verifies no synchronous first-draw generation and proxy/static
preparation budgets. CI asserts workloads rather than machine-dependent timings.

Remaining work includes selected/viewport prioritization, finer scheduling within
a single large GDTF/upload, incremental world bounds, dependency-aware OBJ/GLB
cache identities, exact geometry CPU/GPU sharing and factoring local translations
into runtime transforms. These must preserve canonical nodes, matrices and export.
