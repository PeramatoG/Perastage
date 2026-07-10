# GDTF mode and channel browser

Checkpoint 08E2 replaces the previous flattened read-only mode-channel text with a hierarchical, read-only browser.

## Hierarchy

The non-GUI reader in `core/gdtf/gdtf_mode_channel_browser.*` parses `description.xml` into this source-order tree:

```text
DMXMode
└── DMXChannel
    └── LogicalChannel
        └── ChannelFunction
            ├── ChannelSet
            └── SubChannelSet
```

Every node receives a deterministic identity built from the mode and typed sibling indexes, so GUI expansion, selection restoration, and tests do not depend on names or memory addresses.

## Raw and effective values

The core model keeps raw source text separate from calculated values. Omitted raw attributes remain empty and are displayed as `Not specified` by the GUI adapter. Effective DMX and physical ranges carry origin metadata such as explicit, inherited, calculated, and unavailable.

## DMXValue normalization

The parser accepts plain integers, `value/n`, and `value/ns` forms with whitespace trimming, overflow checks, byte-count validation, and optional parent-channel resolution. Mirror values keep the parsed byte value. Shift values normalize into the parent resolution when a parent resolution is known.

## Range calculation

Channel Function ranges preserve XML order. A function starts at its own valid `DMXFrom`; its end is the next valid sibling start minus one, or the parent channel maximum for the final function. Channel Set ranges use the same rule inside the parent function range. Invalid, duplicate, descending, or out-of-parent starts produce structured diagnostics while preserving the original nodes.

## Attribute units

The reader resolves exact Attribute names through `AttributeDefinitions/Attributes/Attribute`. The browser displays the official `PhysicalUnit` and exposes `Pretty`, `Feature`, `ActivationGroup`, and `MainAttribute` in the details inspector. Units are never inferred from names.

## Parser and GUI boundary

Core parsing has no wxWidgets dependency and does not use GUI diagnostics. The GUI uses `gdtf_mode_browser_presenter.*` to format columns and detail rows, and `gdtf_mode_data_view_model.*` to expose read-only `wxDataViewModel` items. No TinyXML2 nodes or archive handles are exposed to reusable panels.

## Caching

`FixtureEditDialog` owns the active parsed document cache and reloads it when the active GDTF source changes or an apply operation produces a new source. Changing only the selected mode reuses the cached document.

## Read-only status and 08E3 extension points

08E2 intentionally does not implement wheels, gobos, color swatches, sliders, live DMX, simulation, editing, or XML writing. Wheel and attribute-rich inspection remains reserved for Checkpoint 08E3.

## Quick channel summary

The Fixture editor keeps a compact read-only Mode channels summary below Physical properties for fast scanning. It is presentation-only and is populated from the same cached hierarchical mode document as the browser. The browser column labelled `Channel function` shows readable logical/function channel names when available; raw offset and break values remain in the details inspector.

The quick summary intentionally expands multi-byte channels into separate physical channel rows, such as `1: Pan` and `2: Pan Fine`, while the hierarchical browser shows the grouped channel function text on the parent DMX Channel row. The data-view model enables container columns only for root channel rows, so values such as `Pan, Pan Fine` appear next to `DMX Channel 1, 2` without forcing every nested container to redraw all columns. The presentation normalizes reference-like channel function labels, so paths such as `Yoke_Pan.Pan.Pan` display as `Pan` and expand to fine/ultra-fine byte rows when required.
