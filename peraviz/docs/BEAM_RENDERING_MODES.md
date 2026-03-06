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
- Runtime rotates the beam mesh by **+90° on X** so the cone projects along fixture **local -Z** (same direction as `SpotLight3D`).
- Runtime places the beam center at `position = Vector3(0, 0, -beam_range * 0.5)` so the cone starts near the lens and extends forward.

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

- Beam intensity range: `beam_multiplier` now supports `0.0 .. 20.0` to recover volumetric beam visibility in large scenes.
- Volumetric renderer uses a stronger internal intensity scale and non-squared alpha path so the cone remains visible while preserving gobo modulation.

- Volumetric and legacy radial attenuation now use cone-local geometric radius (not a single UV axis), avoiding directional over-fade that could hide the beam.

- Beam mesh node rotation uses `-90°` around X so cone local axis aligns with fixture local `-Z` (same emission direction as spotlight).
