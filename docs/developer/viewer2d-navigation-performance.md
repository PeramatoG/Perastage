# 2D viewport navigation and rendering performance

The investigation started from `012a2ad9` (version 1.7.31). The published
change builds on `20355ec9` (version 1.7.49), after the unified typography
merge and the vertical pan fix. Fixture labels use the same font catalog,
shaped glyphs and text commands in the interactive viewer, Layout editor
and document capture.

## Confirmed causes

- Pan motion wrote five saved viewport values through `ConfigManager::SetFloat`.
  Wheel and keyboard navigation took the same write path. A changed value
  incremented the project render revision. On the next lightweight controller
  poll, a new render revision invalidated visibility candidates and scheduled
  resource synchronization. Settling the interaction could also schedule an
  unconditional controller update.
- The composed fixture label cache cleared on a new render revision or a change
  to the serialized fixture-label overrides. A camera change consequently
  caused all eligible labels to be composed again. Editing one fixture's
  style also discarded warm entries for other fixtures.
- The composed-label path received no interactive-mode policy. It visited all
  visible-layer fixtures and issued two projections per glyph before accounting
  for the viewport.
- Before the typography merge, the legacy renderer submitted one NanoVG frame
  per fixture stack and repeated its text measurements. After the merge,
  legacy fixture settings use canonical composition. The renderer was still
  resolving stack baselines and font line metrics on cache hits.

## Camera and label changes

Live pan offsets and zoom stay on the panel. A drag persists the final position
on button release or capture loss. Wheel and keyboard changes persist when
input settles. The active project's save and dirty-prompt paths flush pending
navigation, and Fit to Scene persists immediately. Existing saved keys, value
normalization and project dirty tracking still apply. The camera, view
orientation and 2D render mode keys advance the presentation revision alone.

Pan uses the live physical pointer inside the canvas with the event position
as a fallback outside it. The final drag sample comes from the release event.
Camera movement invalidates pointer hit results and placement alignment. During
a temporary pan, clipboard placement realigns once after navigation instead
of moving the scene object on camera paints. The existing wxWidgets redraw
queue still coalesces input.

The composition cache retains entries by fixture UUID and compares label
values, view orientation, projected anchor bounds, and the resolved field
styles. A scene revision triggers pruning of deleted UUIDs. A new override
document supplies resolved style inputs for each entry at lookup. Changing
one fixture or field leaves unrelated cache entries warm. An anchor change
recomposes the fixture only if it changes the bounds used to place the text.
A new font catalog invalidates compositions. The typography layer shares
glyph contours across fixtures. A warm cache hit skips stack positioning,
font line metrics and shaping. Pan and zoom only change the mapping of those
commands to framebuffer pixels.

The screen pass tests the composed field's text envelope, including font
bearings, accents, the screen-space outline and rasterizer padding. The test
uses the label's bounds even if its fixture is outside the viewport. It
keeps the whole field when its envelope touches an edge. The projection
retains Bottom's reversed anchor axis and the existing unmirrored glyphs.
The renderer projects each surviving glyph once, reuses NanoVG font IDs and
keeps the outline-then-fill order, including the shared typography raster
path and vector glyph fallback. Document capture takes the full path and
retains offscreen text commands for the document renderer to clip or place.

## 2D and 3D pipeline differences

Both viewports use the shared resource controller, bounds cache, and visible
set. 3D also pumps progressive resource work independently of paint and can
resume a pending cursor when movement settles. 2D services pending work from
paint, defers it during interaction, and drains it for capture. After the
camera fix, movement alone no longer starts a resource generation or dirties
the scene's depth-sorted lists. A new 2D orientation still requires an
orientation-specific depth order. A changed camera can still require a new
frustum-visible set for trusses and scene objects. Fixtures remain candidates
for geometry because resource bounds can be conservative or incomplete.

The 3D renderer also has adaptive mesh representations and optional moving
frame work, including outline deferral. 2D intentionally takes the authored
mesh path where a symbol is unavailable. Its mesh edges can affect the
orthographic profile, picking and document output. Measure that path with
mesh-heavy scenes before changing it. If it dominates, a candidate is a
cached, screen-only orthographic edge representation keyed by source mesh
revision, orientation, relevant instance transform and physical pixel scale.
Retain silhouette and hard edges, admit it only within a measured
framebuffer error tolerance, and use authored data for capture and picking.
The current renderer uses authored geometry for these paths.

The remaining work to measure includes the fixture scan and style-key
lookups, frustum testing under pan, geometry and outline submission on
mesh-heavy scenes, and ruler, grid, support labels and hover. Cold label
composition still runs for offscreen fields whose ink envelope is unknown.
The renderer still submits individual NanoVG glyph draws after culling.
A spatial index or glyph batching change would need separate profiling and
edge and outline comparisons.

## Repeatable benchmark

1. Build the original and corrected revisions with the same build type and
   dependencies. Start both with the same preferences, renderer, display
   scale, viewport dimensions and VSync setting. Use separate runs for 2D
   and 3D to isolate each viewport’s swaps.
2. Use two saved copies of representative MVR scenes. Use a small plot with
   about 25 fixtures and a mixed large plot with 500–1,000 fixtures, trusses
   and authored mesh objects. Keep the same project and resource files
   between revisions. Record the fixture count, approximate mesh complexity,
   number of styled labels and resolved GDTF resources. Include names,
   fixture IDs and DMX addresses. Save a second configuration for a
   geometry-heavy run where the render mode draws detailed meshes.
3. Warm the project until resource loading completes and labels have been
   drawn. Run 30 seconds of middle-button back-and-forth pan, then 30 seconds
   of alternating wheel zoom at the same starting scale. Use the same
   pointer path and event timing on both builds. Include a path that moves
   most labels outside the view. Repeat each run three times. Record cold
   startup and settle frames separately. For the 3D reference use the same
   scene, an orthographic view when available and a comparable visible area.
4. Diagnostics are off by default. Set `PERASTAGE_VIEWER2D_PERF=1` before
   launching the corrected build.
   The application writes one `Viewer2D performance` report per 120
   presented frames and a final partial batch on a normal close. Each
   report includes mean, nearest-rank p95, p99 and maximum frame times with
   and without swap. Input-to-swap measures the oldest camera update included
   in each presented frame up to return from the buffer swap. Display scanout
   and compositor latency require external measurement. Stage timings are
   counts and CPU-side milliseconds, and nested stages overlap. Glyph counts
   describe submitted fixture-label ink. All timer samples come from the CPU side.
5. Compare both builds and the 3D reference with the same OpenGL swap trace
   or external frame-time recorder. Use its per-frame samples to compute
   the run mean, p95 and maximum. Keep CPU submission times, swap intervals
   and end-to-end display metrics in separate columns. When comparing the
   corrected build's application logs, combine batch means by sample count
   and retain the largest maximum. Keep p95/p99 per batch unless the tool
   also captured individual frame samples. Avoid hardware-dependent CI gates.
6. Check the corrected run's warm pan and zoom batches for zero composed
   label rebuilds, scene invalidations, resource syncs or re-sorts caused by
   camera input. Move and rename a fixture and change one field's typography.
   Its composition should rebuild with a matching cause. Pan offscreen and
   back and compare the culled and submitted label/glyph counts. Repeat with
   grid and ruler off to isolate their contribution.

## Linux Debug benchmark on the 1.7.31 base

These single-run software OpenGL measurements, from Xvfb and Mesa llvmpipe,
compare the original 1.7.31 checkout with the first implementation before
integration with unified typography. Both builds opened the same saved projects, with 24 or 600
fixtures using the bundled Dummy 1ch GDTF. The projects use styled names
with accented characters, IDs, DMX addresses and outlines. The framebuffer
was 586 × 582 pixels. A physical middle-button pan ran for 18 seconds at
30 pointer samples per second. The trace measures time between entry into
consecutive `glXSwapBuffers` calls for the viewport, excluding the first two
seconds of the gesture. These figures describe repaint cadence on this
software-rendering setup.

| Styled fixtures | Build | Intervals | Mean (ms) | p95 (ms) | Worst (ms) |
| ---: | --- | ---: | ---: | ---: | ---: |
| 24 | Baseline | 306 | 51.9 | 91.7 | 190.6 |
| 24 | Corrected | 465 | 34.2 | 42.3 | 128.4 |
| 600 | Baseline | 23 | 685.5 | 781.5 | 825.7 |
| 600 | Corrected | 102 | 154.6 | 199.2 | 218.4 |

The 3D perspective reference used the same 600-fixture scene. Across
911 swap intervals, the mean was 17.5 ms, p95 was 22.8 ms and the worst
interval was 102.3 ms. This reference used 3D's own camera, canvas size
and hover-label path.

For the 600-fixture project, the first corrected 120-frame diagnostics
batch recorded 600 compositions, each a cold entry, and 59,349 offscreen
label rejections. Label submission and flushing took 16.7 seconds in that
batch, compared with 2.2 seconds for the scene. The warm 24-fixture batches
recorded no composition rebuilds, resource syncs or re-sorts. After startup,
the resting 2D viewport crops matched pixel for pixel in the sampled,
downscaled screenshots from both project sizes.

On the 1.7.31 base, the Linux Debug application built successfully and
41 focused CTest cases passed, including the saved-project round trip, label composition and
culling, viewport coordinate math, drag and placement policy, shared
typography, and PDF serialization. Translation and repository guardrails
passed. The live Print Viewer 2D action generated a readable A3 PDF of
the 24-fixture project with visible names, IDs and DMX addresses. Native
Windows and macOS runs, hardware OpenGL measurements, and full visual
parity across the Layout editor, SVG and PDF remain to be verified.

## Visual and interaction checks

- Pan and zoom in Top, Front and Side, and in Bottom where supported. Reopen
  the saved project and verify the last position and zoom. Keep long and
  accented labels with thick outlines just inside each edge, including a
  text field offset into the viewport from a fixture outside it.
- Select an object after panning with the pointer stationary. Drag fixtures,
  try constrained and snapped movement, and check hover on fixtures, trusses
  and objects. Start single and batch clipboard placement, pan temporarily
  with the left or middle button, then resume snapping and confirm or cancel.
- Compare the editor, Layout viewport, 2D SVG and PDF with identical label
  settings and paper bounds. Include mixed legacy and composed fields and
  both regular and bold text. Exercise the capture while a navigation
  interaction is pending.
- Run the Debug tests on Windows, Linux and macOS. Test on at least one
  high-DPI display and with fractional scaling where the platform offers it.
