# Beam rendering modes

Peraviz now provides two beam rendering modes in **Visual Settings**:

- **Volumetric (default)**: realistic haze shaft rendering with distance attenuation, view-dependent scattering, soft end fade and quality presets.
- **Lightweight (legacy)**: previous cone-based beam rendering for low-resource systems.

## Gobo in beam behavior

- Surface footprint projection remains unchanged and still relies on `SpotLight3D.light_projector`.
- Beam gobo visibility is implemented in beam shaders by modulation, using a gobo-aware material cache keyed by projector texture RID.
- Cached beam materials update immediately when beam gobo settings change, without rebuilding fixture beam nodes.

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
