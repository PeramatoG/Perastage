# Peraviz gobo support

This document summarizes how Peraviz reads and applies gobos from GDTF fixtures.

## Parsing flow

- Peraviz reads `description.xml` from each `.gdtf` archive.
- Wheel images are discovered from `FixtureType/Wheels/Wheel/Slot` (and exporter variant `WheelSlot`).
- `Slot/@MediaFileName` is resolved as a resource stem and loaded from the archive, preferring:
  - `wheels/<MediaFileName>.png`
  - plus compatible fallback paths (`wheels/images/...` and stem-based archive lookup).
- DMX-to-slot mapping is parsed from `ChannelFunction/ChannelSet` using:
  - `ChannelSet/@DMXFrom`
  - `ChannelSet/@DMXTo` (optional, inferred from next range when missing)
  - `ChannelSet/@WheelSlotIndex`

## DMX binding rules

- Gobo binding focuses on selector channels (`Gobo1`, `Gobo1Pos`, etc.).
- Non-selector channels are ignored for projection (`Spin`, `Shake`, `Time`, `Speed`, `Rotate`, etc.).
- Per-fixture bindings include all discovered gobo selector wheels (`gobo_wheels`) and keep wheel `1` mirrored as compatibility keys (`gobo1_*` / `gobo_*`).

## Runtime projection in Godot

### Primary mode: shadow cookie (default)

Peraviz now follows the same principle used in Godot proposal #11987-style setups:

- A small `QuadMesh` is placed near the spotlight lens.
- The quad uses an alpha-scissor shader (`gobo_occluder.gdshader`) and casts shadows.
- The spotlight shadow map carries the gobo cutout into surfaces and volumetric fog.
- `SpotLight3D.light_projector` stays disabled in this mode to avoid mixing two different gobo systems.

This mode is the default because it aligns the in-air volumetric look with the footprint on geometry.

### Alternative mode: projector cookie (optional)

Peraviz keeps `SpotLight3D.light_projector` support as an explicit alternative mode (`projector_cookie`).
Use it only when intentionally testing projector behavior.

### Additional notes

- `SpotLight3D.shadow_enabled` must be `true` for shadow-cookie gobos.
- For malformed/missing media, a temporary fake gobo texture can be generated for DMX/debug validation.
- When multiple gobo wheels are active, Peraviz composes them into a single mask texture by multiplying wheel masks.
- Compatibility renderer (`gl_compatibility`) can produce different or limited results; Forward+ is recommended.


## Hard beam troubleshooting (in-air gobo visibility)

If the footprint looks correct on the floor but the beam in air looks too soft, these values are the main drivers:

- `SpotLight3D.spot_attenuation`: lower values (around `0.5`) produce a tighter/harder beam edge, similar to the #11987 sample.
- `SpotLight3D.shadow_blur`: keep low (`~0.1`) for crisp cookie edges in volumetric fog.
- `WorldEnvironment.volumetric_fog_density`: start near `0.01` and increase gradually if needed; too much density quickly blooms the whole cone.
- `SpotLight3D.light_volumetric_fog_energy`: start near `1.0` and raise carefully; high values can over-bloom the in-air beam.

Peraviz now applies those shadow-cookie defaults when gobos are active in `shadow_cookie` mode, while keeping them user-tunable from Visual Settings.

- Keep volumetric beam extent aligned with `SpotLight3D.spot_range`; if those diverge, the fog cone can look much larger than the cookie footprint.
