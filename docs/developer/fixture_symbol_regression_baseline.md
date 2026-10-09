# Fixture-symbol regression baseline

## Structural baseline

The baseline in `tests/fixtures/fixture_symbols/` freezes the current top,
bottom, front, and left-side symbol structure. Each text file records the view
identity, calibrated bounds and dimensions, calibration offsets (the bounds minimum),
stroke width, ordered fill polygons and holes, ordered strokes, and the complete
serialized SVG including its `viewBox` and translated coordinates.

The test constructs a 14 by 12 RGBA image directly in memory. A blue,
asymmetric stepped silhouette, an off-centre cut-out, and a black line vary by
view, making rotation and reflection mistakes visible without loading an image
file. It then calls the production `Symbol2DImageBuilder`, geometry simplifier,
GUI-independent physical calibration core, and `ExportSymbolToSvgString`.
Consequently the test needs no OpenGL driver, GPU, font renderer, image decoder,
or filesystem timestamps.

Snapshot structure uses deterministic production ordering. The review layer
uses the classic C locale and rounds structural floating-point values to four
decimal places (a tolerance of 0.00005 physical units); complete production SVG
text is also retained. An intentional update must be made by reviewing and
editing the four checked-in text files in the same change as the production
change. The test has no golden-file update mode. Reviewers should compare view
orientation, bounds and offsets, point order and counts, and SVG coordinates,
not merely a digest.

Production contour extraction stores directed boundary edges in a sorted vector
and indexes outgoing edges with an ordered map. Traversal starts from the first
unconsumed edge and prefers the turn that keeps filled pixels on the right,
followed by straight, left, and reverse continuations with the destination as an
explicit tie-breaker. Implicitly closed rings preserve winding and rotate to the
lexicographically smallest `(x, y)` vertex. Outer polygons sort by descending
absolute area and then their full canonical sequence; holes sort by their full
canonical sequence without changing ownership.

Closed-ring simplification uses the canonical first vertex and its
deterministically selected farthest vertex as anchors. The two forward ring
chains between those anchors are simplified independently with the existing RDP
epsilon and merged without duplicate anchors. This makes simplification
invariant under cyclic input rotation without treating open strokes as rings.
The four baselines were updated for canonical point rotations and the reviewed
deterministic removal or retention of seam-adjacent vertices. Their bounds,
dimensions, offsets, polygon/hole topology, stroke width, and SVG view boxes are
unchanged; contour tests additionally verify exact extracted filled area.

This is a structural rather than pixel baseline because GPU rasterization and
driver behaviour are not portable across Linux, Windows, and macOS. It freezes
the deterministic pipeline after capture while leaving optional real-renderer
tests free to provide additional integration coverage.

## Capture-view contract

Runtime capture consumes `FixtureSymbolCapturePlan()`, which has this stable
order:

1. Front viewer view to front symbol, without horizontal mirroring.
2. Top viewer view to top symbol, without horizontal mirroring.
3. Side viewer view to left/side symbol, with horizontal mirroring.
4. Top viewer view with the bottom override to bottom symbol, without horizontal
   mirroring.

## Timing diagnostics

`FixtureSymbolTimings` is an optional, independently owned model based on
`std::chrono::steady_clock`. Disabled collection does not read the clock inside
phase scopes. The formatting contract produces one compact fixture work record with the
following stable phase order. Collection is optional; disabled sinks do not
read clocks inside phase scopes or submit a timing record. The helper does not
own preparation, project persistence, or GDTF completion policy.

- `resolve`: deterministic GDTF path resolution.
- `fingerprint`: source-content hashing and cache validation-request assembly.
- `inspect`: inspection of the existing archive and symbol set.
- `bounds`: fixture geometry-bound resolution before viewport sizing.
- `capture`: the four existing GUI-thread `RenderToRGBA` calls, including each
  view setup and required side mirror.
- `vectorization`: `Symbol2DImageBuilder` conversion and existing geometry
  simplification.
- `calibration`: physical-unit calibration after capture.
- `archive_rewrite`: the retained diagnostic field for intentional standard
  GDTF resource mutation and canonical atomic publication. Project-symbol Apply
  stores PSTG data independently and never rewrites GDTF.
- `validation`: exact stored-view inspection before a runtime skip, plus
  post-replacement entry and semantic-fingerprint validation. Repeated scopes
  accumulate for generated work.
- `refresh`: the existing scene refresh after a successful apply.

An absent phase is formatted as `-`; the programmatic accessor returns zero for
that phase. Repeated phase scopes accumulate. `total_us` is wall-clock elapsed
time for the fixture work record and can therefore exceed the phase sum.

A stored-SVG-valid **skipped** job contains resolve and exact-view validation;
an inspection skip also contains inspect. A **generated** job contains all applicable phases,
including bounds through refresh and any separately requested publication phases. A **failed** job keeps
every phase completed before its failure; later phases remain absent. Outcomes
are deliberately limited to `skipped`, `generated`, and `failed` by the current
contract.
