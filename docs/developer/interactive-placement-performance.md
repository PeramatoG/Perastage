# Interactive placement and drag performance

Audited on Perastage `main` at `fed3291e`, after the 2D camera and label
performance changes. The interaction paths cover native placement, single
clipboard placement, rigid clipboard batches, and ordinary selection drags.

## Confirmed causes and changes

| Work | Before | After |
| --- | --- | --- |
| Presentation | A pointer handler could call wxWidgets `Update()` and wait for an immediate paint and buffer swap. | Both viewers queue paint. The latest valid cursor sample drives motion, and button press and release positions are processed before placement is committed. |
| Resource synchronization | 2D continuous placement bypassed the movement grace period. 3D primarily deferred heavy work during camera movement. | Applying a transform refreshes the 2D grace period and independently records 3D transform activity. Pending resource work can resume after input settles. Finishing the gesture invalidates the moved bounds and sort order. |
| Notifications | 2D and 3D sent synchronous `Scene` notifications during movement. The visible DMX Patch could schedule a snapshot rebuild. The Position Tool separately scheduled a refresh. | One `TransformPreview` notification is available after a successfully presented preview. The Position Tool coalesces live feedback. DMX Patch ignores previews and still receives structural changes. The table views reconcile at the existing completion boundaries, replacing the 2D snapshot worker. |
| Magnet | Fixture-to-truss candidates re-resolved world-space paths every move. The overlay independently resolved the same paths on paint. | Fixture queries and overlays share a per-gesture path cache. The scene render revision, scene replacement and a target transform change invalidate the relevant data. Candidate ranking, thresholds, view weighting and grouping still use the existing algorithms. |
| Projection | 3D placement and dragging repeatedly bound the GL context and read the same projection matrices, including separate reads for Magnet. | 3D reuses a projection from the same camera pose, framebuffer dimensions and content scale. |
| Transactions | Clipboard code copied scene state when opening, confirming or cancelling a provisional placement. The first actual move of an existing selection could create its Undo snapshot. | The existing transaction boundaries and snapshot ownership are retained. Duplicate movement samples do not start a history operation or rerun Magnet. |

During a preview, moved trusses or scene objects can cross a frustum edge while
the previous bounds are still cached. In that case the viewport conservatively
includes all layer-visible trusses and objects. Fixture-only drags retain normal
culling, which already keeps fixtures after layer and type filtering. Selection
highlights and labels remain available. Camera movement retains its own
render-quality policy. Document capture keeps its regular synchronization path.

## Repeatable runtime profile

Use the same machine, build type, display refresh rate, rendering preferences,
view size and local MVR/GDTF files for baseline and corrected runs. Use scenes
with about 50, 500 and 2,000 fixtures. Include one with many trusses and one with
many instances of the same GDTF. Let the scene load and warm up, then repeat each
gesture five times for at least 5 seconds.

1. Start the corrected build with `PERASTAGE_PLACEMENT_PERF=1` in its
   environment. The application log emits aggregate `transform performance`
   lines after 120 presented previews or on gesture completion. Use the same
   build diagnostics settings in both runs. On 2D, `PERASTAGE_VIEWER2D_PERF=1`
   also reports label and visibility costs.
2. In both viewports, drag an existing fixture and a mixed selection. Paste a
   single fixture and a batch, move them, and click quickly after a final
   movement. Repeat with Magnet enabled and disabled and with the DMX Patch
   visible and hidden. Repeat with slow and fast strokes and two viewport
   sizes. Exercise both the Top/Front/Side/Bottom 2D views and 3D orbit and zoom
   during an active placement.
3. Record gesture type, scene size, applied transforms, presented previews,
   and average, p95, p99 and maximum frame times with and without the swap.
   Record the latest-applied-transform-to-swap time and the call counts and
   elapsed totals for resource work, bounds, GL binding, Magnet resolution
   and evaluation, live position refresh and patch snapshots. The elapsed
   latency is an application-side proxy. It starts after applying the newest
   transform and ends when the CPU returns from the swap. A high-speed camera
   or OS presentation tracing can measure physical cursor-to-display latency
   under the same settings on both builds.
4. The baseline does not emit the new placement line. Compare both builds
   using the same external timing tool, or port just the timer and counters
   to a local baseline build. Keep any baseline synchronous paint in the
   pointer timing region. Check invocation counts independently of timings.
   Patch snapshots should stay flat during movement. Magnet cache misses
   should track target or scene changes, while evaluation continues for
   each changed fixture position.
5. Confirm the exact final positions and snap result, cancel a second preview,
   and exercise Undo/Redo. Verify labels, highlights, insertion points,
   hidden layers, visible bounds, parent relationships and fresh UUIDs.
   Repeat after replacing the project and while the Position Tool is visible.

No hardware frame-time comparison was collected for this audit. Deterministic
tests cover coalescing, placement activity and settling, live Position Tool
updates, and Magnet result equivalence plus cache-miss counts. The manual
profile above covers display latency and the larger scene matrix.

## Remaining work to measure

Fixture snap evaluation still visits every truss, and truss-to-truss candidate
resolution and overlay projection can still be expensive in crowded scenes.
The 3D view-plane projection still unprojects individual samples. 2D and 3D
both continue drawing their ordinary geometry and overlays. Profile these
costs before adding spatial indexing or changing geometry representations.
The conservative visibility policy can increase draw work when moving
non-fixture objects in a scene with many offscreen meshes.
