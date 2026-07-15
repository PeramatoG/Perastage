# GDTF Editor UI Layout

Checkpoint 08E1 refines Fixture Edit and Truss Edit layout only. It does not change GDTF parsing, apply transactions, edit-session ownership, adapters, undo ordering, preview loading, metadata parsing, mode parsing, or channel parsing.

## Shared visual conventions

- GDTF editor dialogs use `gui/gdtf/gdtf_editor_visual_metrics.h` for DPI-aware margins, pane gaps, section spacing, compact form spacing, minimum pane sizes, preview height, and action-row margins.
- Reusable GDTF sections use a flat title, divider line, and content host instead of nested static-box borders.
- The reusable `GdtfEditorPanel` remains presentation-only and owns exactly one metadata, type identity, physical properties, and modes panel.
- Dialog layout persistence is host-owned through `gui/gdtf/gdtf_editor_layout_preferences.*`, not inside reusable child panels.

## Fixture Edit layout

Fixture Edit uses nested splitters:

1. `contextSplitter`: left compact Fixture instance pane vs. the rest of the workspace.
2. `visualSplitter`: GDTF workspace vs. visual-resource pane.
3. `GdtfEditorPanel` internal splitter: overview pane vs. Modes and Channels workspace pane.

The Fixture instance pane is a vertically scrollable compact form titled `Fixture instance`. It is intended to open around 300 logical pixels wide and remains user-resizable.

The Fixture GDTF section order is configured with typed placements:

- Overview pane: Fixture type, GDTF metadata, Physical properties.
- Workspace pane: Modes and channels.

The visual resources are native notebook tabs:

- `Preview`: 3D Preview in the upper area and Fixture Image below it.
- `Symbols`: the official GDTF SVG thumbnail resource, when available, above the three Perastage-generated Top, Front, and Side symbol views with equal vertical area for the official and generated symbol regions.

The GDTF specification defines the optional `FixtureType` `Thumbnail` resource as a file in the archive root, with `.png` for raster images or `.svg` for vector graphics. The Symbols tab uses that root-level SVG resource as the official symbol preview when present. Switching tabs does not parse GDTF data or recreate the underlying resources.

## Truss Edit layout

Truss Edit uses a compact default height and nested splitters:

1. `contextSplitter`: compact MVR instance pane vs. the right workspace.
2. `GdtfEditorPanel` internal splitter: Truss type and metadata overview vs. a workspace column containing the 3D Preview above Physical properties.

The MVR instance pane is a vertically scrollable compact form titled `MVR instance`.

The Truss GDTF section order is configured with typed placements:

- Overview pane: Truss type, GDTF metadata.
- Workspace pane: 3D Preview host followed by Physical properties.

Modes remain hidden in Truss Edit.

## Metadata and Modes presentation

`GdtfMetadataPanel` now gives the read-only Description field more vertical room and keeps static values wrapped for narrow overview panes. It still displays the unavailable fallback and does not add editing or new parsed fields.

`GdtfModesPanel` remains the existing 08E1 text representation: mode selector, read-only channel count, and read-only multiline channel text. The dedicated workspace pane is the extension point for Checkpoint 08E2, where the mode/channel browser can replace the text control without redesigning the surrounding dialog.

## Persistence and clamping

Layout preferences use normalized splitter ratios, dialog sizes, and the selected two-tab Fixture visual pane under independent keys:

- Fixture: `gdtf_editor/fixture/dialog_width`, `dialog_height`, `context_ratio`, `visual_ratio`, `gdtf_ratio`, and `visual_tab`.
- Truss: `gdtf_editor/truss/dialog_width`, `dialog_height`, `context_ratio`, `preview_ratio`, and `gdtf_ratio`.

Saved sizes are clamped to the current display work area before restore. Splitter ratios are clamped so panes cannot restore fully collapsed after monitor or DPI changes.

## Responsive and DPI expectations

All new spacing and minimum-size constants are converted with `FromDIP()` through the shared metrics helper. The intended minimums keep the dialogs usable at approximately 1366x768 while scaling cleanly at 100%, 125%, and 150% DPI.

## Next checkpoint

Checkpoint 08E2 is the Mode and Channel Browser. It will replace the current text-based mode/channel representation with a hierarchical browser, while preserving the host-owned session and apply boundaries established through Checkpoint 08D and kept unchanged in 08E1.
## Checkpoint 08E2 mode browser

The Fixture editor Modes and Channels section now hosts a read-only hierarchical `wxDataViewCtrl` and a read-only details inspector behind an internal horizontal splitter. The browser shows Item, DMX range, Physical range, and Unit while preserving the 08E1 outer splitters, compact fixture pane, visual notebook, Truss layout, preview tabs, and action buttons. The nested browser/details splitter persists through the host-owned `gdtf_editor/fixture/mode_browser_ratio` preference. Truss Edit continues to hide Modes.

Checkpoint 08E2 also keeps a compact legacy-style read-only Mode channels summary in the overview column immediately below Physical properties, so users can quickly scan channel content without expanding the hierarchical browser. The browser no longer includes the experimental Channel function column; channel-function text remains in the quick summary where it is useful for scanning. The presentation normalizes reference-like channel function labels, so paths such as `Yoke_Pan.Pan.Pan` display as `Pan` and expand to fine/ultra-fine byte rows when required.

## Wheel and Attribute Inspector layout

The lower GDTF mode browser area may be split into details, active mapping, preview, and ordered WheelSlot gallery regions. Only normalized layout ratios are persisted through the existing layout-preferences helper. Slider values and resolved resources are not project data. Resource previews are loaded lazily and cached by source, archive entry, and target size.

## Fixture visual-column wheel page

Fixture Edit exposes the Wheel and Attribute Inspector in the visual column as a `GDTF wheels` notebook page. The page shows the active DMX mapping and an ordered wheel-slot list while preserving the existing Preview and Symbols pages.

## Wheel preview thumbnails

The `GDTF wheels` visual-column page includes an active preview area and an ordered thumbnail list. Gobo and graphic-wheel media are shown as static images, while color/filter-only slots are shown as approximate swatches.
