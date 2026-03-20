# Peraviz Coordinate System and Transform Auditing

This document explains how Peraviz maps MVR/GDTF transforms into Godot runtime space, and how to audit that mapping with deterministic logs.

## 1) Input units and applied conversion

### Source units
- MVR/GDTF transform positions are read as millimeters in the source matrix translation (`o`).

### Runtime units
- Peraviz runtime positions are meters.

### Exact conversion in code
- Position conversion is:
  - `x_m = x_mm / 1000`
  - `y_m = z_mm / 1000`
  - `z_m = -y_mm / 1000`
- Implemented by:
  - `map_position_mm_to_m(source_mm) -> Vec3{source_mm[0] / 1000, source_mm[2] / 1000, -source_mm[1] / 1000}`.

### Audit log token
When coordinate debug mode is enabled, runtime prints metadata with:
- `scale_global=mm_to_m(0.001)`

## 1.1) Godot viewer import magnification (runtime-only)

To support visibility checks in the standalone Godot viewer, imported external scene content is currently scaled at the `Proxies` root in `load_scene.gd`:

- `IMPORTED_CONTENT_SCALE = 10.0`
- Applied in `_apply_imported_content_scale()` with:
  - `proxies_root.scale = Vector3.ONE * IMPORTED_CONTENT_SCALE`

Important:
- This magnification is a **viewer/runtime presentation multiplier**.
- Native transform parsing and unit mapping remain unchanged (`mm -> m` conversion in coordinate mapper).
- Use this when validating dense/large rigs that are hard to inspect at default camera distances.

## 2) Source/destination handedness

### Destination (Godot runtime)
- Runtime explicitly declares destination as right-handed with:
  - `godot_handedness=right_handed`
  - `godot_up=+Y`
  - `godot_forward=-Z`

### Source -> destination axis remap
- Source vector remap used before basis composition:
  - `(x, y, z) -> (x, z, -y)`
- This remap is reported in debug metadata as:
  - `source_to_godot=(x,y,z)->(x,z,-y)`

### Local basis composition
After source vector remap:
- `out.u = mapped_u`
- `out.v = mapped_w`
- `out.w = -mapped_v`

This is done with the documented intent to preserve parent/child composition correctness (`C * R * C^-1` mapping path).

### Runtime application rule for imported basis
- When native data provides `has_basis=true`, `load_scene.gd` now applies that basis
  directly to `Node3D.transform` (without multiplying by a second scale).
- Rationale: imported basis vectors already encode the original local scale/mirroring,
  so re-applying an extra scale can distort mirrored model instances.

## 3) Runtime axis convention (up and forward)

Peraviz runtime uses Godot conventions:
- Up axis: `+Y`
- Forward axis: `-Z`

In debug axis gizmos:
- X axis is drawn red.
- Y axis is drawn green.
- Z axis is drawn blue toward `Vector3.BACK` (Godot +Z), while expected beam direction reference remains local `-Z`.

## 4) Beam convention (`-Z local`) and visual validation

### Convention
- Expected beam direction reference is local `-Z` for emitter geometries.
- During GDTF fixture expansion, emitter-like geometry emits a coordinate-debug event with:
  - `expected_beam_direction_local=-Z (GDTF reference)`

### Visual validation method (no mandatory screenshots)
Use the built-in coordinate debug mode:
1. Enable coordinate debug (project setting `peraviz_debug_coords` or press `C` in the scene).
2. Load an MVR scene with fixtures that contain emitter geometry.
3. Confirm debug legend appears with:
   - `[PeravizCoordDebugLegend] ... beam_expected_local=-Z`
4. Inspect emitter gizmos (orange origin marker) and local axes (RGB) to verify beam-related geometry aligns with the expected local `-Z` convention.
5. Correlate with native emitter log line (`event=emitter_reference`) for the same fixture/geometry id.

## 5) “Por qué el resultado actual es correcto”

The current result is correct when all of the following are simultaneously true and reproducible from logs/runtime state:

1. **Unit correctness is explicit and stable**
   - Coordinate metadata prints `scale_global=mm_to_m(0.001)`.
   - Translation mapping function uses the same fixed `1/1000` scale.

2. **Axis convention is explicit and internally consistent**
   - Coordinate metadata prints `godot_up=+Y`, `godot_forward=-Z`, and `source_to_godot=(x,y,z)->(x,z,-y)`.
   - Basis mapping code follows this same conversion path for all nodes.

3. **Beam reference is explicit and traceable at fixture-geometry level**
   - Emitter geometries print `expected_beam_direction_local=-Z (GDTF reference)`.
   - Runtime debug legend repeats `beam_expected_local=-Z` for visual checks.

4. **Transform output can be audited node-by-node**
   - Baseline debug mode emits `[PeravizBaseline]` logs with both source matrix and mapped runtime transform.
   - Native load summary reports scene counts and debug flags so audits are reproducible per load operation.

Because each critical assumption (scale, handedness/axes, beam local reference) is represented by deterministic runtime logs plus direct mapping code, the result is verifiable without requiring screenshots.

## 6) Debug mode and exact logs for auditing conversions

## Enable flags

### Baseline transform audit
- Project setting key: `peraviz_debug_baseline`
- Load path forwards this flag into native loader.
- Intended output: `[PeravizBaseline]` entries per transform mapping.

### Coordinate-system audit
- Runtime setting key: `peraviz_debug_coords`
- Can be toggled with key `C` in `load_scene.gd`.
- Intended output: `[PeravizCoordDebug]` metadata/events and debug legend lines.

## Exact log signatures to grep

### Core mapping metadata
- Prefix: `[PeravizCoordDebug]`
- Event line includes:
  - `event=coordinate_mapping_metadata`
  - `scale_global=mm_to_m(0.001)`
  - `godot_up=+Y`
  - `godot_forward=-Z`
  - `godot_handedness=right_handed`
  - `source_to_godot=(x,y,z)->(x,z,-y)`
  - `beam_reference_local_direction=-Z`

### Emitter/beam reference
- Prefix: `[PeravizCoordDebug]`
- Event line includes:
  - `event=emitter_reference`
  - `expected_beam_direction_local=-Z (GDTF reference)`

### Baseline transform comparison
- Prefix: `[PeravizBaseline]`
- Fields include:
  - `context=...`
  - `source_local=u=(...) v=(...) w=(...) o=(...)`
  - `mapped=pos=(...) basis_x=(...) basis_y=(...) basis_z=(...) scale=(...) rot_deg=(...)`

### Native scene summary (audit context)
- Prefix: `[PeravizNative]`
- Event line includes:
  - `load_mvr nodes=... baseline_debug=... coords_debug=... fixtures=... trusses=... objects=... supports=... extracted_assets=... cache=...`

### Runtime debug legend and toggles
- Legend enabled:
  - `[PeravizCoordDebugLegend] X=Red Y=Green Z=Blue scene_root_origin=Magenta mvr_instance_root_origin=Yellow gdtf_axis_origin=Cyan emitter_origin=Orange beam_expected_local=-Z`
- Legend disabled:
  - `[PeravizCoordDebugLegend] disabled (press C to enable)`
- Toggle event:
  - `[PeravizCoordDebug] event=toggle coords_debug=...`

## Suggested audit command

From repository root:

```bash
rg -n "PeravizCoordDebug|PeravizBaseline|PeravizNative|PeravizCoordDebugLegend" <your_runtime_log_file>
```

This produces a concise trace to verify scale, axis remap, handedness declaration, beam local reference, and per-node transform mapping.
