# Scene alignment

PR1 supplies scene alignment only. Layout multi-selection/alignment and universal
distribution remain deferred. Existing fixture distribution is unchanged.

## Ownership and command publication

`alignment_geometry` accepts immutable projected rectangles and implements the six
anchors once. A future Layout adapter can supply document rectangles directly.
`scene_alignment` resolves typed chronological scene identities with
`BuildInteractiveTransformTargets` and the existing interactive promotion policy,
collects physical bounds, and passes world-axis translations to `ExecuteBatch`.
The batch validates a complete candidate scene, preserves transform bases and
hierarchy, and publishes through `MutationTransaction` exactly once. A no-op
publishes nothing. Failed validation/loading/publication leaves scene and selection
unchanged. Capture and execution run synchronously on the GUI event thread, so
there is no event yield between geometry capture and commit. Other frontends must
serialize scene mutation on their execution thread too.

The GUI owns controls, localized messages, diagnostics and normal scene/selection
refresh. It performs no bounds or alignment arithmetic. The toolbar remains
accessible in both scene workspaces; actions are disabled in Layout mode. Enablement
resolves identities only and never loads geometry. Publication uses the established
scene refresh boundary; incremental renderer notifications remain owned there.

## Selection chronology

`SelectionState` adds `OrderedSceneSelection`, a runtime vector of typed UUIDs;
vector position is the selection sequence. Category setters reconcile membership:
surviving identities never move, removal deletes the identity, and a later addition
appends it. Thus sorted table/viewer refreshes do not replace chronology. A batch of
new identities in one setter uses ascending UUID order. Calls replacing several
categories use their explicit call order (the existing shared adapters call fixture,
truss, support, scene object, group). This policy makes no claim to individual click
chronology within a rectangle/range selection. Explicit neutral replacement through
`OrderedSceneSelection::Replace` honors caller order, removes duplicates, and does
not reconstruct history from maps. No metadata, dirty state or Undo entry is added
for chronology. Existing Undo snapshots retain order so history restoration does not
change first/last reference identity. Shared command publication uses atomic
`SetSceneSelection` membership synchronization to avoid clearing/re-adding groups. Project replacement clears the selection through existing Reset/load
paths; the ordered-query boundary prunes deleted UUIDs before use.

Normalization preserves the earliest chronological occurrence of each surviving
effective target. A group selected with a descendant or several leaves promoted to
one group produces one target. Suppressed descendants inherit their surviving
group identity using the same interactive target resolver. Missing identities or unresolved
promotion abort with deterministic diagnostics. Reference selection happens after
normalization, so first/last references stay fixed. Explicit group identities are
accepted by the neutral adapter; existing GUI group highlighting/promotion continues
to select groups through their members.

## Bounds and resource ownership

`SceneBounds` is a CPU provider independent of renderer initialization. It reuses
`GeometryBoundsResolver` for 3DS/GLB, the existing cached GDTF geometry tree for
fixtures and GDTF-backed trusses/supports, and the shared renderer resource path
resolver for scene/library resolution. No new resource manager is introduced.
GDTF mesh coordinates are millimeters and node translations are meters; the provider
converts translations once before measuring local fixture bounds. Immutable fixture
bounds have a bounded 128-entry derived cache keyed by resource identity, mode,
file size and modification time. The loader retains canonical archive ownership,
content validation and extraction leases. Mesh cache invalidation remains in the
existing resolver. World projection transforms all eight local AABB corners using
the full matrix. SceneObject and Support geometry-instance matrices are composed
with the node world matrix. Group bounds union descendants' world bounds; cycles,
missing children and multiply owned geometry abort instead of double counting.

Only actual loaded meshes or authored GDTF primitives with complete positive
dimensions qualify. GDTF dimension-box
fallbacks are explicitly marked nonphysical without changing rendering behavior.
Missing resources, absent/empty geometry and non-finite values fail the operation;
there is no implicit insertion-point or invented-size fallback. AABB projection is
conservative for irregular geometry. Dummy supports without physical resources
cannot use geometric alignment. Insertion-point alignment is not exposed by this
PR; it would require a separately named explicit policy.

## Work planes

All calculation uses scene millimeters, independent of UI units and zoom.
Scene alignment uses a 0.01 mm anchor tolerance and predicts the transform
command's float addition before publication. Significant unrepresentable movements
at extreme coordinates abort rather than falsely reporting successful alignment.
The reusable rectangle API accepts an explicit tolerance for document adapters.
The visible selector is authoritative and defaults safely to XY, including in
perspective. It never follows camera yaw. Choose XY for Top, XZ for Front, YZ for
Side; changing viewport orientation does not silently override the selector.

| Plane | Screen horizontal | Screen vertical | Unchanged axis |
| --- | --- | --- | --- |
| XY | +X | +Y | Z |
| XZ | +X | +Z | Y |
| YZ | -Y | +Z | X |

Left/right refer to projected visual edges, including mirrored Side Y. Top uses
maximum projected vertical extent. Only one corresponding world translation
component changes per operation; rotation, scale and the other two coordinates
are preserved. Extents and all deltas use one original immutable bounds snapshot.

## Validation

`SceneAlignment` exercises the pure calculations, mixed chronology, policy
normalization, repeated no-ops and transactional publication failure without GL.
`SceneAlignmentPhysicalBounds` uses production CPU loaders, a real primitive GDTF
archive and a bundled 3DS, including rotated fixtures/trusses, instance transforms,
nested groups, missing resources, invalid transforms and dimension-box rejection.
The existing batch, grouping and MVR interchange regressions remain relevant.
`SceneAlignmentBoundary` guards the GUI-independent geometry/command boundary.
These CTest tests run through the existing platform CI matrix.

## Native toolbar evidence

Linux wxWidgets application smoke test with three real GDTF primitive fixtures,
including different dimensions and two rotated instances. Selection chronology was
fixture 2, fixture 1, fixture 3, despite the fixture table displaying 1, 2, 3.
First-selected Top in XY produced a shared maximum Y of -475 mm; repeating it
created no extra Undo entry. One Undo restored original bounds and Redo restored
the aligned result. Switching to the 2D workspace retained fixture 2 as reference;
Bottom produced a shared minimum Y of -725 mm. X/Z, rotations and scales stayed
unchanged. Canonical GUI MVR export and headless reread retained UUIDs, physical
models, modes, DMX and transformed matrices.

![Shared toolbar and selected aligned fixtures in the 3D workspace](evidence/scene-alignment-3d.png)

![Shared toolbar and selected aligned fixtures in the 2D workspace](evidence/scene-alignment-2d.png)
