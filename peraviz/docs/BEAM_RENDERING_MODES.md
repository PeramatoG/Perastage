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
