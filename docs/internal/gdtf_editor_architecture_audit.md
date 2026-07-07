# GDTF editor architecture audit

This checkpoint documents the current GDTF viewing and editing responsibilities before adding a reusable GDTF editor route. It is based on the code paths reviewed on 2026-07-07 and does not introduce a standalone `.gdtf` startup route.

## Current GDTF read points

- `core/gdtf_metadata_summary.{h,cpp}` reads a `.gdtf` ZIP archive, locates `description.xml`, parses `FixtureType` metadata and the latest `Revision`, and returns a compact read-only summary for UI display. This service was moved from `gui/` during this checkpoint.
- `viewer3d/gdtfloader.{h,cpp}` loads GDTF geometry, modes, channel summaries, fixture name, physical properties, and resources used by fixture preview and 3D loading.
- `core/gdtfdictionary.{h,cpp}` reads GDTF identity and dictionary-related fields, resolves canonical Perastage file names, manages library entries, and creates/updates Perastage library derivatives.
- `core/gdtf_fixture_category.{h,cpp}` infers fixture category from GDTF description data for fixture table population and import/export round trips.
- `core/trussloader.{h,cpp}` reads GDTF truss archives into Perastage truss data and extracts/cache resources for previewable truss content.
- `gui/fixtureeditdialog.cpp` reads thumbnails directly from the GDTF archive for the fixture image preview and uses the shared loader APIs for modes, channels, preview, physical properties, metadata, and embedded symbols.
- `gui/trusseditdialog.cpp` resolves the current truss GDTF path from scene data for metadata display and preview loading.
- `gui/fixturetablepanel.cpp` reads GDTF modes, channel counts, fixture names, categories, physical properties, and stored GDTF paths when rows are loaded or changed.
- `gui/fixtures/fixture_gdtf_resolution.cpp` resolves table/project GDTF references into usable paths for preview and symbol workflows.
- `gui/windows/symbol_fixture_applier.cpp` and related symbol tools load GDTF content while updating Perastage SVG symbols and physical calibration data.
- `mvr/mvrimporter.cpp` reads MVR package contents, resolves embedded or referenced GDTF files, applies import matching rules, and preserves or copies GDTF references depending on import options.
- `main.cpp` routes startup/open-file behavior for current project and MVR paths; no lightweight `.gdtf` route is currently implemented.

## Current GDTF write and mutation points

- `viewer3d/gdtfloader.cpp` contains the approved physical-property mutation helper `SetGdtfProperties`, which writes `Weight` and `PowerConsumption` through the existing mutation audit helpers.
- `core/gdtf_mutation_audit.{h,cpp}` stamps Perastage mutation metadata, adds revision records, and applies physical properties according to the documented mutation policy.
- `core/gdtf_canonicalizer.{h,cpp}` canonicalizes `description.xml` structure for Perastage export rules and can canonicalize one archive into another.
- `core/truss_gdtf_builder.{h,cpp}` creates Perastage-authored truss GDTF archives, writes `description.xml`, adds resources, and appends Perastage revision metadata.
- `core/gdtfdictionary.{h,cpp}` creates or updates Perastage library derivatives so user-library GDTF physical edits can be made without rewriting the original vendor file policy differently.
- `mvr/mvrexporter.cpp` packages GDTF archives into MVR output and patches/canonicalizes GDTF content for compliant export behavior.
- `gui/fixtureeditdialog.cpp` invokes derivative creation and `SetGdtfProperties` only when edited fixture weight/power values actually change.
- `gui/trusseditdialog.cpp` invokes `BuildTrussGdtfFromInstance` when GDTF type fields or cross-section data change.
- `gui/windows/symbol_fixture_applier.cpp` mutates GDTF packages for symbol application workflows covered by existing tests.

## Current GUI entry points

- Edit Fixture is implemented by `FixtureEditDialog` and launched from fixture-table workflows. It combines project row fields, GDTF mode/channel controls, physical-property edits, metadata display, thumbnail display, embedded symbol previews, and 3D preview in one dialog.
- Edit Truss is implemented by `TrussEditDialog` and launched from truss-table workflows. It divides fields into MVR instance controls and GDTF truss type controls, displays metadata, and previews the current truss resource.

## Current project/MVR dependencies in editing code

- `FixtureEditDialog` depends on `FixtureTablePanel`, table row indices, `ConfigManager` through `GetDefaultGuiConfigServices()`, scene fixtures, fixture row UUIDs, undo/dirty/update handling through table-panel services, hoist recalculation prompts, viewer refresh, GDTF dictionary helpers, symbol cache helpers, and preview panels.
- `TrussEditDialog` depends on `TrussTablePanel`, table row indices, scene trusses, `ConfigManager` through `GetDefaultGuiConfigServices()`, project resource path resolution, truss GDTF builder, preview resource resolution, and 2D/3D viewer refresh.
- Fixture and truss dialogs currently know which columns are GDTF type/shared fields and which columns are MVR/project instance fields. That ownership is not yet expressed as a reusable editor context.
- MVR import/export code owns package-level concerns: embedded file extraction/copying, MVR `GDTFSpec` references, canonical names, export patching, import matching, and preservation of external references.

## Current behavior for fixture physical property edits

- Fixture weight and power controls are treated as GDTF type-level physical-property candidates.
- Edits are ignored unless the user modified the relevant columns and the values differ from the original dialog values.
- When the current GDTF path is in the writable user fixture library, the dialog first creates or updates a Perastage library derivative and switches the row to that derivative path.
- The dialog then calls `SetGdtfProperties` with the Perastage `ModifiedBy` stamp.
- On success, matching fixture rows and scene fixtures of the same GDTF/type are updated, physical-property source is set to GDTF, dirty flags are cleared, table values are refreshed, hoist recalculation may be prompted, and existing viewer refresh behavior is preserved.

## Current behavior for truss GDTF generation/update

- Truss fields classified as GDTF type fields are manufacturer, model, length, width, height, weight, and cross section.
- When any GDTF type field changes, `TrussEditDialog` updates the table/scene and calls `BuildTrussGdtfFromInstance`.
- The builder writes a Perastage-authored truss GDTF to the writable truss library using a canonical filename, updates the scene truss `gdtfSpec`, `modelFile`, auxiliary archive path, and default mode, then refreshes the table, metadata, preview, and viewer.

## Current behavior for metadata display

- Fixture and truss dialogs display manufacturer, description, creation date, user ID, modified by, revision, last modified, and version.
- Missing or failed metadata reads show `-` in the UI.
- Metadata reads are read-only and do not rewrite GDTF archives.
- The shared metadata summary service now lives in `core/` and does not include GUI headers or depend on GUI widgets.

## Field classification observed in current dialogs

### GDTF type/shared fields

- Fixture type/name, selected GDTF file/model reference, GDTF mode, channel count derived from mode, category fallback, weight, power, visual color derived from dictionary/type, embedded symbols, thumbnail, and metadata summary.
- Truss manufacturer, model, length, width, height, weight, cross section, generated GDTF path/model file, GDTF mode, preview resource, and metadata summary.

### MVR/project instance fields

- Fixture row/order, UUID, fixture ID, position, universe/address, project label fields, MVR color, table selection, undo/dirty state, row resync, hoist recalculation side effects, and viewer refresh scope.
- Truss row/order, UUID, name/position/project table fields that are not listed as GDTF type fields, scene resource base path, dirty state, and viewer refresh scope.
- MVR import/export fields such as package-local filenames, embedded archive paths, support files, and preservation options are project/package context rather than pure GDTF metadata.

## Risks before a standalone `.gdtf` editor route

- Current dialogs are tightly coupled to table panels and scene state; they cannot be reused without a context object that abstracts row/project operations.
- Physical-property mutation policy depends on library/derivative decisions that are project-context-specific and should not be hidden inside a generic panel.
- Truss generation currently assumes a Perastage truss scene object and writable truss library.
- Preview and symbol workflows use GUI panels and project resource resolution; standalone editing needs explicit resource lifetimes and extraction/cache ownership.
- Import/export canonicalization and mutation policies must remain centralized so standalone reads do not accidentally rewrite external GDTF files.
- Startup routing in `main.cpp` does not yet distinguish a lightweight `.gdtf` route from full project/MVR workspace loading.

## Recommended staged migration plan

1. Continue extracting pure GDTF services into `core/`: archive reading, description lookup, metadata, identity, modes/channels, physical properties, resources, canonicalization, and approved mutation helpers.
2. Introduce a documented `GdtfEditorContext` abstraction with variants such as `StandaloneFile`, `ProjectFixture`, `ProjectTruss`, `FutureProjectHoist`, and `FutureProjectObject`. The context should own write policy, path resolution, dirty/undo behavior, and viewer refresh callbacks.
3. Split reusable UI into a `GdtfEditorPanel` that accepts a context and exposes sections for `Metadata`, `Preview`, `Modes/Channels`, `PhysicalProperties`, `Resources/Symbols/Thumbnail`, and device-specific sections.
4. Adapt `FixtureEditDialog` and `TrussEditDialog` to host the reusable panel while keeping existing table-panel apply semantics in their project contexts.
5. Add the lightweight `.gdtf` startup route only after the shared services and context boundaries prove that metadata reads are non-mutating and mutations require explicit user action and policy selection.

## Verification notes for this checkpoint

- The metadata summary implementation was moved from `gui/` to `core/` and the GUI source list no longer owns that file.
- `FixtureEditDialog` and `TrussEditDialog` only changed their include path for the metadata service; Apply/OK, physical property mutation, truss builder, preview, and viewer refresh logic were not redesigned.
- A small unit test was added for metadata summary loading using a generated in-memory GDTF archive, avoiding persistent binary fixtures.
