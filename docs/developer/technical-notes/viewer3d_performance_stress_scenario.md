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
