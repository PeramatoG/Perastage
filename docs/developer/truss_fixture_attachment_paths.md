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
stable runtime identifier, provenance, estimated thickness and detection
diagnostics. The current resolver considers standardized structure geometry the
preferred future source, followed by GDTF model geometry and geometry supplied by
the MVR representation. In the currently supported import paths, analyzable GLB
or 3DS geometry referenced by the truss supplies the practical input.

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

Each accepted track becomes a two-point polyline in the first version. Snapping
uses the generic `ClosestPointOnPath` operation, so sampled curves, closed paths,
and multi-segment corner paths can be added without changing the snap contract.

## Cache, fallback, and viewers

Local analysis is cached by canonical geometry path, file size, and modification
time. Instance transforms are applied after lookup. Moving or rotating a truss
therefore does not parse or analyze its geometry again, while replacing the
resource invalidates its entry. The cache is process-local and is not serialized.

Fixture snapping uses reliable paths first and records the selected runtime path,
parameter, provenance, and confidence in transient `SnapResult` state. If no
analyzable geometry or stable chord is available, the previous oriented-bounds
surface calculation remains as an explicitly low-confidence
`ApproximateBoundsFallback`. Fixture overlay references sample the same resolved
paths; they do not expose truss connector Magnets. Truss and truss-group overlays
continue to show the independent discrete connector candidates.

The detector currently supports straight trusses with a clearly dominant axis.
Curved, circular, corner, unusually sparse, and heavily occluded geometry will
need structure-aware or curve-fitting resolvers before they can produce reliable
paths and currently use the conservative fallback.
