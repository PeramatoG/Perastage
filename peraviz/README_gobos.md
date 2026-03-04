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

### Geometry fallback mode (beam only, no mixing)

When `Gobo beam visibility` is set to `Geometry shader fallback`, Peraviz now uses a strict
beam-only mode for gobo visibility in air:

- no shadow-cookie projection path,
- no `light_projector` footprint projection,
- gobo texture is applied only inside the volumetric beam shader.

This keeps methods separated and avoids mixed artifacts when comparing approaches.

### Additional notes

- `SpotLight3D.shadow_enabled` must be `true` for shadow-cookie gobos.
- For malformed/missing media, a temporary fake gobo texture can be generated for DMX/debug validation.
- When multiple gobo wheels are active, Peraviz composes them into a single mask texture by multiplying wheel masks.
- Compatibility renderer (`gl_compatibility`) can produce different or limited results; Forward+ is recommended.


## Hard beam troubleshooting (in-air gobo visibility)

If the footprint looks correct on the floor but the beam in air looks too soft, these values are the main drivers:

- `SpotLight3D.spot_attenuation`: lower values (around `0.5`) produce a tighter/harder beam edge, similar to the #11987 sample.
- `SpotLight3D.shadow_blur`: keep low (`~0.1`) for crisp cookie edges in volumetric fog.
- `WorldEnvironment.volumetric_fog_density`: values around `0.02` make the in-air pattern much more legible than very low fog density.
- `SpotLight3D.light_volumetric_fog_energy`: values around `3.0-4.0` help the fog beam read clearly without overexposing the floor footprint.

Peraviz now applies those shadow-cookie defaults when gobos are active in `shadow_cookie` mode, while keeping them user-tunable from Visual Settings.

Current Peraviz defaults are tuned for balanced readability without overexposing the beam volume:

- `volumetric_fog_density = 0.007`
- `light_volumetric_fog_energy = 1.6`
- `shadow_blur = 0.1` for active shadow-cookie gobos
- volumetric fog froxel controls default to `volume_size = 256`, `volume_depth = 96`, `use_filter = true`


### Scale and resolution notes

- Yes: scene scale directly affects how sharp the shadow-cookie appears in fog. If fixture/lens transforms are much larger or smaller than expected meters, the occluder can sample too few shadow texels and look diffuse.
- Shadow-cookie sharpness depends strongly on positional shadow atlas resolution and gobo mask resolution. Peraviz now uses higher runtime gobo mask resolution and higher positional shadow atlas defaults to reduce line artifacts.
- If you still see square fog borders, it is usually volumetric fog froxel resolution. Increase volumetric fog quality settings (`volume_size`, filtering) and reduce additive mesh beam intensity so native fog shadowing dominates.
