# Viewer environment baseline (`test.tscn`)

This document records the conservative default values configured in `Viewer/WorldEnvironment` for artistic iteration and scene-to-scene comparisons.

## Scene rendering policy

- Use a **single default tonemapper**: **ACES** (`tonemap_mode = 3`).
- Keep a **fixed baseline exposure** (`tonemap_exposure = 1.0`) so visual comparisons stay stable.
- Keep **auto exposure disabled by default** (`auto_exposure_enabled = false`) to avoid dynamic re-scaling between takes.
- Use a **highlight-preserving response curve** by setting `tonemap_white = 8.0`, which delays hard clipping and retains more structure in bright beams/lenses.

## Current baseline

- **Background**: neutral solid color (`background_mode = Color`), slightly cool dark gray.
- **Ambient light**: low fill (`ambient_light_energy = 0.2`) with a neutral cool tint to soften hard shadows without flattening depth.
- **Tonemapper**: **ACES** with a fixed baseline (`tonemap_exposure = 1.0`), white point at `8.0`, and auto exposure disabled.
- **Image adjustment**: enabled with moderate values to avoid a washed-out look:
  - `adjustment_contrast = 1.05`
  - `adjustment_saturation = 1.05`

## Suggested iteration ranges

Use these ranges for look-dev passes while preserving a neutral baseline:

- `ambient_light_energy`: `0.15` - `0.35`
- `tonemap_exposure`: `0.9` - `1.2`
- `tonemap_white`: `6.0` - `10.0` (higher values preserve more highlight detail before compression)
- `adjustment_contrast`: `1.0` - `1.15`
- `adjustment_saturation`: `1.0` - `1.15`

Keep adjustments subtle at first and validate against representative MVR scenes before widening ranges.
