# Fixture attachment paths on trusses

## Separate attachment domains

GDTF `Magnet` geometries describe discrete structural connection locations. They
remain the source for truss-to-truss joining and are neither copied nor expanded
for fixture mounting. A fixture can be mounted continuously along a main chord,
so Perastage derives runtime attachment paths from the truss geometry instead.
The derived paths are an application inference, not data defined by MVR or GDTF.

## Resolution and coordinate spaces

`core/truss_attachment_paths` owns the GUI-independent resolver. Its curve-ready
path value contains a local polyline, an instance-transformed world polyline, a
stable runtime identifier, optional estimated thickness and detection
diagnostics. Resolution first reads `truss.gdtfSpec` through the safe read-only
archive reader. It resolves each `Structure`'s named `Model`, composes its full
Geometry/Structure `Position` hierarchy, and tries usable Structure geometry
before other model-bearing GDTF geometries. Only after all usable GDTF sources
are exhausted does it try MVR Geometry3D/Symbol geometry; oriented bounds are
the final fallback. Priority is based on the resource actually being analyzed,
not the mutable `sourceRepresentation` import hint.

The current GDTF standard identifies Structure geometry and its model but does
not explicitly enumerate the individual fixture-mounting chord paths. Therefore
`ExplicitStandardStructure` remains reserved: geometry-derived Structure paths
use `GdtfStructureGeometry`, generic GDTF models use `GdtfModelGeometry`, and the
individual chords remain Perastage runtime inference.

Paths are never written to a GDTF archive, MVR document, `ChildPosition`, project
extension, or any other persistent field. Committing a fixture snap stores only
the existing fixture transform and standard scene group relationship.

## Straight-chord analysis

The first implementation measures a clear dominant bounds axis, then intersects
the indexed mesh with 17 evenly spaced transverse planes. Nearby intersection
points form occupied cross-section regions. Regions are tracked between planes
and accepted only when they persist through at least 70 percent of the truss and
remain transversely stable. This persistence rule rejects short members and
diagonal braces without assuming that a truss has two, three, or four chords.

The analysis operates on the triangles after loader node/object transforms have
been applied. It therefore works for separately authored members and for one
welded or monolithic mesh. Centralized constants in
`truss_attachment_paths.cpp` define axis dominance, sampling, clustering,
persistence, and stability tolerances. Ambiguous dimensions, malformed indices,
non-finite vertices, and unstable tracks fail conservatively.

Each accepted track currently becomes a straight two-point polyline whose span
comes from the track's occupied sample range rather than unrelated global mesh
extents. Radius remains unset unless local geometry can support a reliable
estimate. Snapping
uses the generic `ClosestPointOnPath` operation, so sampled curves, closed paths,
and multi-segment corner paths can be added without changing the snap contract.

## Cache, fallback, and viewers

Local analysis is cached by canonical geometry path, file size, and modification
time. Instance transforms are applied after lookup. Moving or rotating a truss
therefore does not parse or analyze its geometry again, while replacing the
resource invalidates its entry. The cache is process-local and is not serialized.
Each viewer owns its resolver, and analysis requests load vertex/index data without
decoding textures or initializing wxWidgets image handlers. This keeps attachment
resolution safe when an overlay is first requested from a movement event.
If a preferred geometry resource is readable but produces no reliable paths, the
resolver retains its diagnostics and continues to the next eligible source before
selecting the bounds fallback.

Fixture snapping uses reliable paths first and records the selected runtime path,
parameter, provenance, and confidence in transient `SnapResult` state. If no
analyzable geometry or stable chord is available, the previous oriented-bounds
surface calculation remains as an explicitly low-confidence
`ApproximateBoundsFallback`. Fixture overlays project every point and draw continuous segments from the same
resolved polylines; they do not expose truss connector Magnets. Truss and truss-group overlays
continue to show the independent discrete connector candidates.

The detector currently supports straight trusses with a clearly dominant axis.
Curved, circular, corner, unusually sparse, and heavily occluded geometry will
need structure-aware or curve-fitting resolvers before they can produce reliable
paths and currently use the conservative fallback.
