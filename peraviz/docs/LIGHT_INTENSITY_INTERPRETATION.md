# Interpreting light intensity values in Peraviz

This note explains how to read fixture intensity values relative to the final visual output in `test.tscn`.

## Why the same numeric intensity can look different

Rendered brightness is affected by multiple stages, not only by fixture intensity:

1. Fixture/emitter intensity value (source energy).
2. Beam geometry (zoom, lens angle, spread).
3. Distance and incidence angle on surfaces.
4. Scene tonemapping and exposure policy.
5. Post effects (bloom, SSAO perception, material response).

Because of this pipeline, intensity is best interpreted as a **relative control signal** rather than an absolute photometric guarantee.

## Baseline assumptions for comparisons

For reproducible comparisons, Peraviz uses this baseline in `Viewer/WorldEnvironment`:

- ACES tonemapper (`tonemap_mode = 3`).
- Fixed exposure (`tonemap_exposure = 1.0`).
- Auto exposure disabled (`auto_exposure_enabled = false`).
- Highlight-preserving white point (`tonemap_white = 8.0`).

Under this policy, differences you observe across fixtures/scenes are primarily caused by fixture data and scene composition, not by adaptive camera exposure.

## Practical interpretation guide

- Treat intensity values as **comparative**: e.g., fixture A at 80 should appear brighter than fixture A at 40 under unchanged framing.
- Avoid comparing captures made with different exposure or tonemapper settings.
- If lens/beams appear clipped in highlights, first validate that the baseline policy is intact before changing fixture intensity data.
- When evaluating look-dev, report both:
  - Input intensity value(s).
  - Environment settings (`tonemap_mode`, `tonemap_exposure`, `tonemap_white`, `auto_exposure_enabled`).

## Recommended workflow

1. Keep the baseline policy fixed for technical validation.
2. Compare fixtures at multiple intensity points (for example 25/50/75/100).
3. Only after validation, perform artistic adjustments (contrast/saturation/bloom), documenting deviations from baseline.

This process keeps technical comparisons stable while preserving flexibility for final artistic grading.
