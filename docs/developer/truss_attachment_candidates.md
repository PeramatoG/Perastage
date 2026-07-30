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

## Inference

Dimensions are evaluated in truss-local X, Y, and Z. A shape is longitudinal
only when its largest positive finite dimension is **strictly greater than
twice** the second-largest dimension. The strict comparison makes the 2:1
boundary ambiguous while classifying 3000 x 400 x 400 mm as longitudinal.

Longitudinal shapes expose exactly the negative and positive centers of the
dominant-axis terminal planes. Ambiguous shapes, including invalid-dimension
fallbacks, expose the six oriented bounds face centers. These inferred points
are conservative snapping aids, not verified mechanical connectors.

Groups aggregate their truss bounds in the first truss's local basis. Clearly
longitudinal collinear aggregates expose only the two exterior ends, so
internal joints are not normal candidates. Other aggregates use the six-point
ambiguous fallback. Existing hierarchy rejection prevents self and descendant
snaps.

## View weighting

Top and Bottom ignore Z in scoring, Front ignores Y, and Side ignores X. The
3D viewer continues to reduce weights progressively from the camera direction.
Weighting changes selection only; it never removes a component from the final
translation.
