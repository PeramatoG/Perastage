# DMX patch from MVR + Dimmer mapping from GDTF

## Overview

Peraviz now resolves fixture dimmer control directly from loaded MVR and GDTF data:

1. Fixture patch is read from MVR (universe + address, or absolute address).
2. Fixture selected DMX mode is read from MVR (`GDTFMode`).
3. The dimmer channel offset is resolved from the fixture GDTF mode.
4. Art-Net DMX values are applied to fixture dimmer in the Godot runtime.

## Universe offset

MVR universes are often authored as 1-based, while many Art-Net senders expose universe IDs as 0-based.

Peraviz adds a `universe_offset` mapping:

- `artnet_universe_id = mvr_universe + universe_offset`
- Default: `-1` (maps MVR universe 1 to Art-Net universe 0)

You can change this value in the Peraviz DMX control bar at runtime.

## Supported in this implementation

- Art-Net receiver input.
- Fixture patch extraction from MVR.
- Fixture DMX mode extraction from MVR.
- Dimmer channel lookup in GDTF mode.
- Dimmer application to fixtures (existing fixture dimmer pathway).
- Bound and unbound fixture debug summary.

## Not supported yet

- Non-dimmer channel families (color, gobo, shutter/strobe, wheels, prisms, focus, zoom, etc.).
- Multi-attribute fixture playback from GDTF.
- sACN.

## Validation workflow

1. Build Peraviz native with DMX enabled.
2. Load the target MVR that references fixture GDTF files.
3. Enable DMX in Peraviz.
4. Confirm DMX status shows non-zero bound fixtures.
5. Send Art-Net DMX on patched universe/address.
6. Verify fixture intensity responds to DMX dimmer values.

## Notes on unbound fixtures

Fixtures are listed as unbound when one of these happens:

- Missing or invalid patch in MVR.
- Missing selected DMX mode in MVR.
- Missing GDTF reference/path.
- Dimmer attribute not found for the selected mode.
- Resolved dimmer channel index outside the 1..512 frame.
