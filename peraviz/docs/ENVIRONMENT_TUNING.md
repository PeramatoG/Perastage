# Viewer environment baseline (`test.tscn`)

This document records the conservative default values configured in `Viewer/WorldEnvironment` for artistic iteration.

## Current baseline

- **Background**: neutral solid color (`background_mode = Color`), slightly cool dark gray.
- **Ambient light**: low fill (`ambient_light_energy = 0.2`) with a neutral cool tint to soften hard shadows without flattening depth.
- **Tonemapper**: **ACES** with `tonemap_exposure = 1.0` as a safe starting point.
- **Image adjustment**: enabled with moderate values to avoid a washed-out look:
  - `adjustment_contrast = 1.05`
  - `adjustment_saturation = 1.05`

## Suggested iteration ranges

Use these ranges for look-dev passes while preserving a neutral baseline:

- `ambient_light_energy`: `0.15` - `0.35`
- `tonemap_exposure`: `0.9` - `1.2`
- `adjustment_contrast`: `1.0` - `1.15`
- `adjustment_saturation`: `1.0` - `1.15`

Keep adjustments subtle at first and validate against representative MVR scenes before widening ranges.
