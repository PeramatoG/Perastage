# Peraviz gobo support

This document summarizes how Peraviz parses and loads gobo data from GDTF fixtures.

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
- Non-selector channels are ignored for selection (`Spin`, `Shake`, `Time`, `Speed`, `Rotate`, etc.).
- Per-fixture bindings include all discovered gobo selector wheels (`gobo_wheels`) and keep wheel `1` mirrored as compatibility keys (`gobo1_*` / `gobo_*`).

## Runtime loading behavior

- Runtime resolves the active gobo slot from DMX values.
- Slot textures are loaded and cached per fixture.
- When media is missing or invalid, a temporary fallback gobo texture is generated for DMX/debug validation.
- When multiple gobo wheels are active, Peraviz composes them into one cached texture by multiplying masks.
- The composed texture is applied to `SpotLight3D` projector data (engine property detection keeps compatibility across Godot versions).
- The same texture is also sent to the volumetric beam shader, which performs projector-style UV reconstruction inside the cone to approximate fog gobo projection.
