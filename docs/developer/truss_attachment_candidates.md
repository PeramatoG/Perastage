# Truss attachment candidates

Truss-to-truss Magnet snapping is translation-only and uses deterministic
attachment points. Candidate scoring applies the active view's axis weights,
but the selected translation always retains all three components.

## Priority and data

Each truss resolves candidates independently. Usable `Magnet` geometry nodes
from its GDTF archive suppress all inferred points. Their `Name`, optional
`Model`, complete composed `Position` transform, stable traversal identity, and
diagnostic path are retained by a read-only core service. Malformed positions
are diagnosed and skipped. If no usable explicit point remains, inference is
used without writing to the GDTF or MVR data.

Public `GDTFSpec` is resolved first, relative to `MvrScene::basePath` when it is
not absolute. If that reference is absent or cannot be resolved, the Perastage
auxiliary GDTF archive reference is resolved the same way. `modelFile` is never
treated as a GDTF source. Missing and unreadable sources remain visible as
structured diagnostics before conservative inference is used.

An explicit resolver caches type-level local candidates by normalized absolute
archive identity, file size, and a fixed-width nanosecond last-write timestamp.
Instance transforms are
applied after lookup, so dragging or rotating a truss does not reopen its
archive. Replacing an archive changes the version key and reparses its local
definition. Viewers own their resolver lifetime; the core has no static
resource-owning cache.

## Inference

Dimensions are evaluated in truss-local X, Y, and Z. A shape is longitudinal
only when its largest positive finite dimension is **strictly greater than
twice** the second-largest dimension. The strict comparison makes the 2:1
boundary ambiguous while classifying 3000 x 400 x 400 mm as longitudinal.

Longitudinal shapes expose exactly the negative and positive centers of the
dominant-axis terminal planes. Ambiguous shapes, including invalid-dimension
fallbacks, expose the six oriented bounds face centers. These inferred points
are conservative snapping aids, not verified mechanical connectors.

Groups expose candidates resolved from their actual member trusses. Candidate
pairs from different members are treated as occupied internal joints when
their positions are within 25 mm and, when both have inferred directions,
their direction dot product is at most -0.996. Deterministic one-to-one
matching removes occupied candidates. Every remaining member candidate is
exposed with its member UUID and original stable identity. Aggregate face
centers are used only if no member candidates can be built.

Inferred longitudinal ends and ambiguous face centers carry runtime-only
outward directions transformed into world space. Explicit GDTF Magnets do not
receive an invented direction because GDTF does not standardize one.

For a moving truss with exactly two inferred longitudinal ends, target
acquisition remains view-driven. After acquiring one target candidate, both
source ends are evaluated against that same target. If the nearby end would
produce more than 50 mm of substantial oriented-bounds penetration into the
actual target truss or owning group member and the opposite end would not, the
opposite end is selected. Opposing inferred directions break only an
all-overlapping tie. This correction changes translation only and never
rotates or flips the truss.

Cache invalidation normally uses normalized identity, size, and timestamp.
Project/resource lifecycle owners may call `Clear()` when replacing a resource
while deliberately preserving both metadata values; the interactive path does
not hash archives on every query.

## View weighting

Top and Bottom ignore Z in scoring, Front ignores Y, and Side ignores X. The
3D camera-direction weights remain available to the existing non-screen-space
paths. Weighting changes selection only; it never removes a component from the
final translation.

Viewer3D truss-to-truss acquisition instead projects both candidates from one
captured camera snapshot and accepts a separation of at most 16 logical pixels.
Framebuffer coordinates are divided by the content scale, so the aperture is
DPI-independent. Accepted pairs rank by screen separation, absolute view-depth
difference, full world translation length, target UUID, source candidate ID,
and target candidate ID. The 250 mm world threshold is not an eligibility cap
for this Viewer3D path.
