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
- Per-fixture bindings now include all discovered gobo selector wheels (`gobo_wheels`) and keep wheel `1` mirrored as compatibility keys (`gobo1_*` / `gobo_*`).

## Runtime projection in Godot

- Gobo textures are assigned to `SpotLight3D.light_projector`.
- `SpotLight3D.shadow_enabled` must be `true`; otherwise projector textures are not visible.
- For fixtures with malformed/missing media, a temporary fake gobo texture can be generated for DMX/debug validation.
- When multiple gobo wheels are active, Peraviz composes them into a single projector texture by multiplying wheel masks (gobo composition).

## Notes

- Compatibility renderer (`gl_compatibility`) does not support projector behavior reliably.
- Forward+ / Mobile renderers are required for predictable gobo projection.
