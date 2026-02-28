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


## Volumetric gobo geometry tuning

`VolumetricBeamRenderer` now derives gobo projection size directly from beam cone geometry at the occluder plane.
There is no zoom remap multiplier anymore.

You can tune these public renderer properties (or pass the same keys in renderer settings):

- `gobo_occluder_distance_m` (default `0.043`): distance from lens origin to gobo occluder plane.
- `gobo_plane_base_size_m` (default `0.017`): physical side size of the occluder quad mesh before runtime scaling.
- `gobo_footprint_cone_fill_ratio` (default `1.0`): fill ratio applied to cone diameter at occluder depth.

For fixture-specific overrides, set these light metadata keys:

- `peraviz_gobo_occluder_distance_m`
- `peraviz_gobo_plane_base_size_m`
- `peraviz_gobo_footprint_cone_fill_ratio`

The volumetric shader uniforms remain unchanged:
`gobo_size`, `gobo_start_ratio`, and `gobo_texture`.
