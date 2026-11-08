# GDTF channel footprint resolution

`core/gdtf/gdtf_channel_footprint` is the shared read-only address resolver for
Viewer3D's public mode/channel API (including the fixture table's Ch Count) and
the Core mode/channel browser. It resolves the selected mode's top-level geometry
and each reference's unique top-level definition, then matches channel geometry
names to actual elements within each instance, including descendants.
The archive, geometry assignments, rendering and MVR data are not changed.

Following the GDTF 1.2 Geometry Type Reference and DMX Channel sections in
[the local specification](../../reference/gdtf-spec.md), Break children are
associated with the sorted distinct numeric source breaks of physical channels
in that reference's subtree. An Overwrite channel uses the remaining Break
entry. Each entry supplies the effective DMXBreak and DMXOffset (both default to
1). The physical address is the declared channel offset plus DMXOffset minus 1.
Incomplete or ambiguous mappings produce diagnostics and no physical addresses
for the affected channel instance.

Footprints are the highest occupied byte offset **per effective break**, including
all declared coarse/fine/ultra/uber bytes and sparse addresses. Missing/None
offsets are virtual and contribute no physical bytes. Addresses are integers,
not DMXValues: an address above 255 is not an 8-bit value overflow.

The existing scalar channel-count API and browser's calculatedFootprint expose
**break 1 only**, matching the existing single-address fixture table. They never
sum separate breaks or take the maximum across unrelated breaks. The browser
retains `footprintsByBreak` and each channel's `effectiveDmxBreak`; public channel
rows retain `dmxBreak`. A multi-break mode generates a technical diagnostic.
Displaying/editing all break footprints and addresses in the GUI remains a
separate task; the single Ch Count column is not a complete multi-break summary.

For compatibility, files omitting DMXMode.Geometry retain direct channel
addresses, with a diagnostic. Omitted channel geometry retains a direct address
when the mode tree resolves. These are explicit legacy fallbacks, not claims of
standard conformance.

Nested references and channel assignments targeting a GeometryReference node
itself are not expanded. They produce diagnostics instead of guessed addresses.
Missing definitions, non-top-level reference targets, duplicate/ambiguous names,
cycles, malformed breaks/offsets and integer overflow likewise produce
diagnostics. The browser retains unresolved channel source fields for inspection
but exposes no calculated offsets; the legacy API logs the same diagnostics.

Regression tests generate description XML and temporary archives at runtime.
They cover ordinary RGB, repeated referenced roots and child Beams (including
60 RGB pixels with exactly bytes 1–180), sparse and multibyte offsets, virtual
channels, selected-tree scoping, independent breaks, Overwrite, the existing
16-byte GeometryExpanded case, invalid references and source preservation.
This coverage does not establish full GDTF compatibility.
