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

## Expected compatibility matrix

| Mode (`mvr_truss_geometry_authority`) | `GDTFSpec` on Truss | `<Geometries>` on Truss | Visible geometry authority | Expected result in consumers that render GDTF by default |
| --- | --- | --- | --- | --- |
| `MVR_GEOMETRY` (`0`) | Yes | Yes (when symbol geometry is available) | MVR `<Geometry3D>` | No double mesh, truss identity/metadata remains via GDTF. |
| `GDTF` (`1`) | Yes | No | GDTF model | No duplicate mesh from MVR truss geometry. |
