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

Per GDTF, `WheelSlotIndex` is normalized to wheel slot order. Fixtures may also
repeat the same slot across different DMX windows (for example to reuse one gobo
with index/spin/shake behaviors). Runtime range matching therefore keeps fixture
`ChannelSet` declaration precedence: when more than one range matches the DMX
value, the latest matching row wins.

Peraviz accepts both common `ChannelSet` DMX encodings found in real GDTF files:
values authored as absolute DMX positions and values authored relative to the
parent `ChannelFunction` DMX window.

## DMX binding rules

- Gobo binding focuses on selector channels (`Gobo1`, `Gobo1Pos`, etc.).
- Non-selector channels are ignored for selection (`Spin`, `Shake`, `Time`, `Speed`, `Rotate`, etc.).
- Per-fixture bindings include all discovered gobo selector wheels (`gobo_wheels`) and keep wheel `1` mirrored as compatibility keys (`gobo1_*` / `gobo_*`).

## Runtime loading behavior

- Runtime still resolves the active gobo slot from DMX values.
- Slot textures are loaded and cached for each fixture.
- When media is missing or invalid, a temporary fallback gobo texture can be generated for DMX/debug validation.
- When multiple gobo wheels are active, Peraviz composes them into one cached texture by multiplying masks.

> Note: projection/emission logic was intentionally removed from Peraviz runtime. The loaded textures remain available in fixture metadata for upcoming refactors.
