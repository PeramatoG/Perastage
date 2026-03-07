# Beam rendering modes

Peraviz now provides two beam rendering modes in **Visual Settings**:

- **Volumetric (default)**: realistic haze shaft rendering with distance attenuation, view-dependent scattering, soft end fade and quality presets.
- **Lightweight (legacy)**: previous cone-based beam rendering for low-resource systems.

## Performance guidance

- Use **Volumetric + Low** on integrated GPUs.
- Use **Volumetric + Medium/High** on desktop GPUs for better smoothness and turbulence detail.
- Switch to **Lightweight (legacy)** at runtime when you need maximum FPS.

## Quality presets

- **Low**: fewer raymarch steps, no turbulence noise.
- **Medium**: balanced cost and visual quality.
- **High**: highest step count and full turbulence contribution.

## Beam axis conventions

The volumetric and legacy cone meshes follow the same axis convention:

- Cone geometry is authored along **local +Y/-Y** (`CylinderMesh.height` axis).
- Runtime rotates the beam mesh by **+90° on X** so the cone projects along fixture **local -Z** (aligned with spotlight projection axis).
- Runtime places the beam center at `position = Vector3(lens_shift_x, lens_shift_y, -(beam_range * 0.5 + near_offset))` so the cone starts near the lens and extends forward along `-Z`.

Shader shaping uses:

- **Axial factor** from local mesh Y to control near-lens intensity and far-end fade.
- **Radial factor** from local XZ distance to keep the center denser than edges.

## Optical single-source parameters

Peraviz keeps one optical parameter set in `load_scene.gd` and shares it with both branches:

- `beam_angle_deg`
- `beam_range`
- `gobo_rotation_deg`
- `gobo_scale`
- `lens_offset_m`
- `near_offset`
- `lens_shift_x`
- `lens_shift_y`
- `beam_softness`
- `beam_radial_falloff`
- `beam_longitudinal_falloff`
- `beam_intensity` (`scaled_intensity`)
- `haze_density_multiplier`

The same `gobo_texture` (already transformed with rotation/scale in `FixtureGoboProjector`) is sent to:

1. `SpotLight3D.projector` for footprint projection.
2. Beam cone shaders for visible in-air pattern.

### Geometry coherence

Beam mesh aperture is derived from spotlight optics:

- `radius_at_distance = tan(beam_angle_deg * 0.5) * distance`
- `bottom_radius = tan(beam_angle_deg * 0.5) * beam_range`

Beam length matches `beam_range`, and center offset uses `lens_offset_m`.

### Gobo projection in beam shader

Beam shaders do not rely on mesh UV unwrapping for gobo sizing. They project from cone local coordinates and normalize by projected cone radius at each depth sample.

This keeps zoom/angle and gobo scaling behavior aligned with footprint projection.


### Defaults and tuning notes

- Beam intensity range: `beam_multiplier` supports `0.0 .. 100.0` with default `20.0`. The first `0..20` span preserves previous visual calibration, while `20..100` adds overdrive headroom for large scenes.
- Volumetric renderer uses a stronger internal intensity scale, aggressive overdrive gain above 20, and a non-zero visibility floor so the cone remains visible in low-haze scenes.

- Volumetric and legacy radial attenuation now use cone-local geometric radius (not a single UV axis), avoiding directional over-fade that could hide the beam.

- Beam mesh node rotation uses `+90°` around X so cone local axis aligns with fixture local `-Z` (same emission direction as spotlight).

## Optical control traceability (gobo footprint + in-air beam)

### Runtime ownership map

- **Master optical parameters**: `BeamOpticsController.BuildBeamParams()` and `BuildDefaultMasterOptics()`.
- **Surface footprint projection**: `FixtureGoboProjector.apply_gobo_projection()` sets the same gobo texture into `SpotLight3D.projector/light_projector`.
- **Visible beam in air**:
  - Volumetric beam path: `beam_renderers/volumetric_beam_renderer.gd` + `shaders/volumetric_beam.gdshader`.
  - Legacy beam path: `beam_renderers/legacy_cone_beam_renderer.gd` + `shaders/legacy_beam_cone.gdshader`.
- **Optional atmospheric support**: `FogVolumeGoboBeamController` + `shaders/fog_volume_gobo_beam.gdshader`.

### Fixed mismatches addressed

- Beam renderers now consume `gobo_scale` and `gobo_rotation_deg` from master optics instead of hardcoded `1.0/0.0`.
- Beam shaders sample gobo outside-UV as zero (no clamped edge stretching), removing square border artifacts.
- Haze defaults were lowered to keep gobo detail visible in-beam; volumetric fog is now supplemental, not the primary detail source.

### Tunable master parameters

- `beam_angle_deg`, `beam_range`
- `gobo_rotation_deg`, `gobo_scale`
- `lens_offset_m` / `near_offset`
- `beam_softness`
- `beam_radial_falloff`, `beam_longitudinal_falloff`
- `beam_intensity` (`scaled_intensity`)
- `haze_density_multiplier`

These are intended as the single optical truth for both footprint and visible beam.


### Legacy gobo baseline contract (do not break)

Legacy mode currently provides the reference gobo look and must remain unchanged unless a dedicated regression pass is included.

Baseline behavior in legacy (`legacy_cone_beam_renderer.gd` + `gobo_prism_mesh_builder.gd`):

- Uses vectorized gobo aperture geometry (`GoboPrismMeshBuilder.build_beam_mesh`) instead of a pure texture mask over a generic cone.
- Applies the same transformed gobo texture prepared by `FixtureGoboProjector` (`peraviz_gobo_texture` metadata).
- Keeps orientation compatibility with:
  - `beam_rotation_deg = gobo_rotation_deg + 180°`
  - `MIRROR_BEAM_SHAPE_X = true`
- Uses shared optics from `BeamOpticsController` (`beam_range`, `beam_angle`, `gobo_scale`, `gobo_rotation_deg`, `lens_offset_m`, `lens_shift_x`, `lens_shift_y`).

Any future refactor should preserve those four points to avoid visual regressions in fixture gobos.

## Volumetric gobo status

- Problem observed: if volumetric uses legacy mesh-cut gobo geometry directly, some fixtures can show thin line artifacts and poor beam volume continuity.
- Current volumetric strategy: keep a full cone mesh and apply gobo shaping in shader projection using the same transformed gobo texture from `FixtureGoboProjector`.
- Volumetric keeps legacy orientation compatibility by applying `beam_rotation_deg = gobo_rotation_deg + 180°` and X-mirroring in gobo projection math.
- Surface footprint projection remains unchanged and still comes from `FixtureGoboProjector` through spotlight projector texture.

This keeps volumetric beam volume readable while retaining legacy-consistent gobo orientation and shared optics controls.

## Visibility safeguard for shader-masked paths

When a renderer path uses shader-side gobo masking, keep a minimum transmission floor (`gobo_min_transmission`) and blend control (`gobo_contribution`) to avoid full beam disappearance with dark gobos.

For volumetric beams, also ensure beam alpha is explicitly intensity-aware so beam visibility tracks dimmer/beam intensity changes.

For two-sided volumetric cones (`cull_disabled`), view alignment should use absolute normal alignment (`abs(dot(NORMAL, VIEW))`) to avoid face-orientation cancellations that can make the beam disappear from many camera angles.
