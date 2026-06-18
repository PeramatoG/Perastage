# MVR Truss Geometry Authority

Perastage now exports truss objects with an explicit geometry authority policy controlled by the `mvr_truss_geometry_authority` configuration key.

## Configuration

- `mvr_truss_geometry_authority = 0` → `MVR_GEOMETRY` (default)
- `mvr_truss_geometry_authority = 1` → `GDTF`

## Export policy

The exporter always keeps `GDTFSpec` for truss objects when a GDTF source is available (existing truss GDTF, `.gdtf` model file, or generated GDTF).

To prevent duplicate meshes in downstream consumers, only one source is allowed to provide visible geometry per truss:

- In `MVR_GEOMETRY`, visible geometry is written in the MVR `<Geometries>` node and the truss GDTF is rewritten as metadata-only (no renderable model).
- In `GDTF`, no truss `<Geometries>` node is written and visible geometry is expected to come from the referenced GDTF.

## Truss type and instance metadata

Standard truss type properties use the referenced GDTF as their primary
exchange representation:

- manufacturer and model names are stored on `FixtureType`;
- dimensions are stored on `Models/Model`;
- weight is stored in `PhysicalDescriptions/Properties/Weight`;
- visual resources are referenced by `Models/Model/@File`.

Perastage continues to read older `TrussInfoMap` entries containing copies of
these fields, but new exports do not duplicate standard GDTF properties there.
Perastage-only metadata remains in root
`GeneralSceneDescription/UserData/Data[@provider="Perastage"]`, keyed by the
canonical truss UUID. No `UserData` child is written directly below `Truss`.

The optional truss Load value is Perastage instance metadata rather than GDTF
type data. An absent or empty `Load` entry means automatic load calculation.
A non-empty manual override is written as:

```xml
<TrussInfo uuid="...">
  <Load unit="kg" source="Manual">123.45</Load>
</TrussInfo>
```

Clearing the Load table cell restores automatic mode and removes this element
from subsequent exports. Manual values are highlighted in red in the truss
table.

Generated truss GDTFs retain the simple top-level
`Geometry Name="Root" Model="Main"` representation. Perastage does not require
the optional GDTF `Structure` geometry in order to avoid changing compatibility
with consumers that already render these visual-only truss fixture types.

## Expected compatibility matrix

| Mode (`mvr_truss_geometry_authority`) | `GDTFSpec` on Truss | `<Geometries>` on Truss | Visible geometry authority | Expected result in consumers that render GDTF by default |
| --- | --- | --- | --- | --- |
| `MVR_GEOMETRY` (`0`) | Yes | Yes (when symbol geometry is available) | MVR `<Geometry3D>` | No double mesh, truss identity/metadata remains via GDTF. |
| `GDTF` (`1`) | Yes | No | GDTF model | No duplicate mesh from MVR truss geometry. |
