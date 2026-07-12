# GDTF Wheel and Attribute Inspector

Checkpoint 08E3 adds a read-only inspection architecture for wheel, filter, media, graphic-wheel, and DMX value resolution. The feature is split so core parsing and resolving remain independent from wxWidgets, project data, sessions, Apply, undo, and XML writing.

## Architecture summary

- `core/gdtf/gdtf_wheel_catalog.*` reads Wheels, WheelSlots, Filters, graphic-wheel metadata, CIE colors, prism facets, animation-system labels, exact names, source order, and stable IDs from `description.xml`.
- `core/gdtf/gdtf_color_cie.*` preserves raw CIE xyY source text and provides an approximate display-only sRGB preview conversion.
- `core/gdtf/gdtf_dmx_inspector.*` resolves a selected DMX channel and normalized value through the 08E2 mode/channel model without depending on GUI, project, session, or viewer code.
- `ReadGdtfArchiveResource` reads one bounded archive entry lazily without extracting the full GDTF or creating temporary files.
- `gui/gdtf/gdtf_resource_bitmap_cache.*` is GUI-only and decodes image bytes into bounded `wxBitmap` thumbnails. It contains no XML parsing and no session or project logic.

## Wheel, WheelSlot, Filter, and graphic-wheel model

The catalog preserves exact GDTF names and source order. Wheel slots use a 1-based effective index. Slots preserve raw `Color`, parsed CIE xyY, raw `Filter`, exact `MediaFileName`, resource references, prism-facet metadata, animation-system metadata, and graphic-wheel references. Graphic wheels are classified separately when the GDTF type, name, or slot metadata identifies them, so they are not treated as ordinary gobos when the source distinguishes them.

Filters preserve exact names, stable IDs, raw CIE values, parsed CIE values, and source order. Exact references are used first. Missing references are reported with structured diagnostics instead of guessed.

## Resource lookup and cache lifecycle

Resource loading is lazy. The archive reader resolves the requested entry, supports unambiguous compatibility fallback, rejects unsafe paths, enforces a byte limit, preserves UTF-8 paths, and returns raw bytes plus diagnostics. Core code does not decode images.

Fixture Edit can cache the parsed mode/channel document, wheel/filter catalog, archive inventory, resource bytes, and GUI thumbnails per active source. That cache must be cleared when the active GDTF source changes, Browse selects another file, Apply produces another source, the session is rebound, or the source becomes unavailable. Changing mode, selecting a node, or moving the inspection value must not reopen the archive.

Truss Modes remain hidden and truss editing must not load wheel resources.

## CIE preview limitation

The CIE model keeps raw values unchanged. xyY values are validated, converted to XYZ, transformed to display sRGB, transferred through the sRGB curve, and clipped safely. The swatch is only an approximate monitor preview and is not a replacement for the raw GDTF color value or a spectral simulation.

## DMX resolver behavior

The resolver uses the effective DMX ranges calculated by the 08E2 mode/channel browser. The deterministic mapping is:

```text
DMX value -> active ChannelFunction -> active ChannelSet -> ChannelFunction.Wheel -> ChannelSet.WheelSlotIndex -> exact WheelSlot -> media, color, filter, or graphic-wheel resource
```

It preserves multiple LogicalChannels in source order, decomposes normalized values into bytes, resolves active ChannelFunctions and ChannelSets by containment, exposes the active Function and Set DMX ranges for UI state feedback, validates `WheelSlotIndex` as 1-based, and never guesses a slot from names, labels, physical values, or ChannelSet order. ModeMaster results are marked conditional. DMXProfile references make physical output approximate or unavailable. Increasing and decreasing physical ranges are interpolated only when numeric values are reliable.

## Read-only and future editing boundary

The inspector state is typed and read-only: selected channel, current inspection value, active function/set, wheel, slot, resource, and diagnostics are represented without exposing mutable XML nodes. Checkpoint 08E3 does not add editable cells, Save, dirty state, undo commands, add/delete/reorder behavior, XML writing, live DMX, animation playback, prism rendering, or project/session mutation.

## Connected Fixture Edit presentation

Fixture Edit now hosts a read-only `GDTF wheels` page in the visual column. Selecting a DMX channel, LogicalChannel, ChannelFunction, or ChannelSet in the modes browser chooses the owning DMXChannel for inspection. Moving the DMX inspection slider shows the exact resolution-aware DMX value and percentage, calls the pure resolver, and updates the active mapping label with the active Function and Set ranges plus the wheel panel with Function, Set, Wheel, 1-based Slot, media, filter, graphic resource, ModeMaster, DMXProfile, and diagnostics. The panel lists the resolved wheel slots in source order and emphasizes the active slot; it remains read-only and does not write XML, send DMX, or mutate the edit session.

## Visual previews

The Fixture Edit `GDTF wheels` page now resolves selected WheelSlot media and graphic-wheel resources through the lazy archive reader, decodes them through the GUI bitmap cache, and displays thumbnails in the ordered slot list plus a larger active-slot preview. Slots without usable media show an approximate CIE xyY swatch when slot or filter color data is available. Missing or unreadable resources keep a safe placeholder and status text.

The wheel catalog accepts both `Slot` and legacy `WheelSlot` child elements so fixtures using standard wheel slot markup can populate the visual gallery and DMX slot resolver.

Extensionless wheel media names are resolved through canonical wheel locations first, including `wheels/<name>.png`, before compatibility fallbacks. This keeps common GDTF `MediaFileName` values such as `99015494-09(1)` connected to their PNG archive entries.

## Preview decode diagnostics

The GUI bitmap cache returns an explicit decode result instead of treating every placeholder bitmap as a successful image. `hasThumbnail` and `hasActivePreview` are set only when wxWidgets decodes an actual image; empty data, unsupported image bytes, invalid dimensions, oversized images, and decode failures remain visible as status text without being reported as successful previews.

Decoded wheel images are aspect-fit into the requested preview size, preserve alpha, and are composed over a neutral checkerboard background so black-and-transparent gobo artwork remains visible in dark application themes. The cache key includes a source fingerprint based on the canonical archive path, file size, and last-write timestamp so replacing a GDTF at the same path invalidates stale previews.

Selecting a row in the Wheel slots gallery previews that slot directly with a larger preview bitmap and shows its resource-resolution or decode diagnostic without changing the read-only DMX inspection value. Media slots that cannot decode a thumbnail show the themed placeholder rather than a misleading color swatch.

Standard GDTF 1.2 resolution prefers `Slot/@MediaFileName` and `wheels/<MediaFileName>.png`. Non-standard graphic-wheel resource attributes are documented compatibility metadata and are attempted only after the standard media resource is missing or cannot produce a decoded preview.

When restored projects or MVR imports provide an already-exploded GDTF resource folder, Fixture Edit passes the cached GDTF extraction directory from the loader into the core resource reader. The reader can also probe folders beside the `.gdtf` archive as a read-only compatibility fallback after the archive lookup reports the resource missing. This fallback reads only the requested file, keeps path containment checks, and reports the fallback in the slot diagnostic.
