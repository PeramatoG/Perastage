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

## Checkpoint 02 read-only archive/document boundary

Checkpoint 02 adds a small read-only core foundation that separates archive access from semantic `description.xml` parsing:

- `core/gdtf_archive_reader.{h,cpp}` owns opening a `.gdtf` archive, normalizing archive-relative entry names, inventorying entries, locating `description.xml` case-insensitively, detecting missing or duplicate case-insensitive description entries, and returning exact description XML text with structured diagnostics. It never extracts files permanently and never writes to the input archive.
- `core/gdtf_description_reader.{h,cpp}` owns the basic serialization-neutral document snapshot for root `DataVersion`, `FixtureType` identity fields, thumbnail references, ordered revisions, ordered DMX mode names, ordered wheels, and ordered wheel slots with resource references. Public types use standard C++ containers and strings and do not expose GUI widgets, project state, MVR state, or mutable XML node pointers.
- The archive implementation still privately depends on wx ZIP streams because this checkpoint intentionally avoids a new archive dependency. That dependency is isolated to `gdtf_archive_reader.cpp` and is not exposed through public headers.

`core/gdtf_metadata_summary.cpp` is now a presentation adapter over these read services. It no longer owns direct ZIP traversal or direct `description.xml` archive lookup, and the existing Fixture Edit and Truss Edit metadata fields keep their prior fallback behavior and timestamp formatting.

## Duplicate GDTF readers that remain after Checkpoint 02

The following read responsibilities were identified during the review and remain intentionally out of scope for this checkpoint:

- `viewer3d/gdtfloader.cpp` still owns temporary extraction, `description.xml` parsing, geometry/model loading, DMX modes/channels, fixture names, and physical-property reads through functions such as `LoadGdtf`, `GetGdtfModes`, `GetGdtfFixtureName`, and `GetGdtfProperties`.
- `core/gdtfdictionary.cpp` still opens GDTF archives in its dictionary identity and canonical filename path.
- `core/gdtf_fixture_category.cpp` still opens and parses `description.xml` to infer fixture categories, including wheel/beam/signals used by category heuristics.
- `core/trussloader.cpp` still opens GDTF truss archives, extracts resources, and reads truss-specific description data.
- `core/symbols/PerastageSvgSymbol.cpp` still reads full ZIP entry maps and parses `description.xml` for embedded symbol workflows.
- `gui/fixtureeditdialog.cpp` still has a direct thumbnail reader for preview image display and still calls viewer-loader APIs for modes, channels, physical properties, and preview content.
- `mvr/mvrimporter.cpp` and `mvr/mvrexporter.cpp` still own MVR package ZIP traversal, embedded GDTF resource handling, export patching/canonicalization, and import/export-specific reference resolution.

These paths should migrate incrementally only where the new read contract fits their ownership. MVR package behavior, mutation, derivative naming, and rendering remain separate responsibilities.

## Perastage/Peraviz semantic compatibility rules

The new read-layer public contract is intentionally serialization-neutral so it can remain compatible with Peraviz without sharing GUI or project implementation details. The shared rules for future expansion are:

- Prefer official GDTF/MVR specification semantics over Perastage-specific assumptions.
- Represent repeated GDTF families as ordered collections with stable identity instead of fixed singleton fields.
- Preserve unknown/custom data as unread semantic data or diagnostics; do not reinterpret vendor extensions as standard fields.
- Keep archive reads non-mutating: no canonicalization, renaming, derivative archive creation, or rewrite behavior belongs in the read layer.
- Keep editor state, project dirty state, undo/redo, runtime rendering, and MVR instance/package state outside the core document snapshot.

## Gobo regression fixture and follow-up evidence

`tests/gdtf_read_services_test.cpp` includes a generated archive/XML regression with two distinct gobo wheels, multiple ordered slots per wheel, and separate media references. This protects the new read-only snapshot from collapsing, overwriting, or reordering wheel and slot collections.

The missing-gobo runtime symptom is not fixed in this checkpoint. Evidence from the current code review shows that runtime gobo display remains downstream of `viewer3d/gdtfloader.cpp` geometry/resource extraction and channel/wheel-slot handling, especially `LoadGdtf`, the extracted `description.xml` path handling, and channel-set `WheelSlotIndex` labeling. A safe next migration target is to extract a read-only resource/reference view from `viewer3d/gdtfloader.cpp` behind the new archive/document services before changing rendering or DMX runtime behavior.

## Checkpoint 02 verification notes

- Added focused tests for valid archives, case-insensitive `description.xml` lookup, missing and duplicate description entries, malformed XML, missing root/FixtureType, ordered revisions, ordered DMX modes, repeated gobo wheels/slots, unknown elements, metadata-summary compatibility, and Unicode archive paths/entry names.
- Added `tests/check_gdtf_metadata_summary_boundary.sh` to keep `gdtf_metadata_summary.cpp` from reintroducing direct ZIP traversal or direct `description.xml` archive lookup.
- No GDTF or MVR write path was changed.


## Checkpoint 03 editing-domain boundary update

Checkpoint 03 introduces non-GUI GDTF editor architecture types under `core/gdtf/editor/`. The new layer formalizes a read-only `gdtf::GdtfDocument`, separate `gdtf::GdtfEditableValues`, `gdtf::GdtfEditSession` dirty tracking and validation, explicit `GdtfSourceKind` and `GdtfWritePolicy` values, field ownership descriptors, standalone/project fixture/project truss context adapters, and GUI-independent apply request/result structures.

The exact current Fixture Edit and Truss Edit field ownership classifications, ambiguous field decisions, context adapter behavior, and remaining unmigrated dialog responsibilities are documented in `docs/internal/gdtf_editor_context_boundaries.md`. Existing Fixture Edit and Truss Edit runtime behavior remains unchanged in this checkpoint: apply logic, table updates, approved physical-property mutation, truss GDTF generation, previews, dirty state updates, viewer refreshes, and hoist/load prompts are still owned by the existing GUI/table-panel code paths.

The next approved checkpoint remains reusable panel extraction from the existing dialogs. Startup routing, standalone `.gdtf` windows, `.gdtf` command-line opening, installer file associations, raw XML editing, new GDTF attributes, rendering changes, and MVR/GDTF write-path replacements remain out of scope.

## Checkpoint 03A correction: editability and dirty tracking

Checkpoint 03A fixes the editor-domain model before reusable GUI panel extraction. The root issue was that the field registry used one boolean for several different concepts: whether a value is editable in an existing host dialog, whether it is a GDTF document value, whether it is a context selection, and whether the common edit session can store it. Because `SetEditableValue(...)` also had a generic presentation-value fallback while dirty tracking used a separate fixed list, host/project fields could be accepted without becoming dirty.

The corrected model introduces explicit field value kinds and per-context capabilities. Ownership remains about where a field belongs, value kind describes what the session value represents, host-dialog editability describes current dialog behavior, and context capability is the final authority for session visibility, editability, and apply categorization.

Final context support is intentionally conservative: project fixtures expose weight and power as document edits plus fixture type/name presentation, selected mode, and source GDTF reference as context selections; project trusses expose manufacturer, model, dimensions, weight, and cross section as type/generation fields plus source/model reference as a context selection; standalone contexts are read-only by default and future editable standalone policy only enables genuine document/type values. Selected mode is not modeled as a GDTF mode-definition edit, and source reference is not modeled as a document mutation.

Apply requests now separate document changes from context-selection changes. Existing Fixture Edit and Truss Edit dialogs, write paths, preview behavior, table updates, undo/redo, hoist recalculation, viewer refresh, physical-property mutation, and truss GDTF generation remain unchanged.

The next recommended checkpoint is to extract the first reusable GDTF metadata panel.

## Downloaded GDTF insertion preparation correction

The downloaded-GDTF insertion failure predated Checkpoint 03A: the field-capability changes did not touch `MainWindow::AddFixtureFromGdtfPath(...)` or the GDTF Share download route. The add-to-project path still used legacy `viewer3d/gdtfloader.cpp` helpers for identity and mode discovery. Those helpers extracted archives into a temporary directory with throwing filesystem operations and then assumed the selected description was exactly `<temp>/description.xml`, so environmental filesystem failures or non-canonical but unambiguous archives could fail without useful application diagnostics.

The corrected path prepares fixture insertion with a non-GUI read-only service before opening `AddFixtureDialog`. Identity and ordered DMX modes now come from the shared archive/description services; invalid, ambiguous, unsafe, or mode-less GDTFs produce structured diagnostics and a concise user-facing failure instead of silently returning. The workflow logs validation, archive-read, description-read, mode-read, dialog-opened, inserted, and failed stages without logging credentials or full catalog payloads.

Read/import tolerance is now explicit: canonical root `description.xml` wins; one root case-insensitive match is accepted with a compatibility warning; one unique nested candidate is accepted only when no root candidate exists; ambiguous or unsafe candidates fail. Reading and insertion remain non-mutating and do not canonicalize downloaded files. Explicit Perastage mutation, generation, canonicalization, and export paths remain responsible for standards-compliant root `description.xml` output.

Final PR #2125 validation restored the legacy fixture-power fallback in the shared read-only description reader: positive explicit `PowerConsumption` values remain authoritative, and consumer `WiringObject` `ElectricalPayLoad` or `ElectricalPayload` values are summed only when no positive explicit power exists. Wheel slot `MediaFileName` now resolves as a preserved basename under canonical `wheels/` resources with `.png` or `.svg` extensions, with precise missing, ambiguous, and case-compatibility diagnostics. Empty or whitespace-only selected `description.xml` entries are now diagnosed as `EmptyDescriptionXml` while preserving the selected archive path in diagnostics. ZIP Unicode filename recovery when the UTF-8 flag is absent remains a separate deferred compatibility task.

## Checkpoint 05A reusable metadata panel start

Checkpoint 05A starts the reusable GUI migration without creating the complete `GdtfEditorPanel`. `gui/gdtf/GdtfMetadataPanel` now owns only the read-only metadata presentation for manufacturer, description, creation date, UserID, ModifiedBy, revision, last modified, and version. It keeps unavailable or empty values displayed as `-`, preserves the multiline read-only description control, and handles reversible value wrapping inside the panel from stored unwrapped presentation values.

Metadata source-path resolution remains owned by the existing host dialogs. `FixtureEditDialog` still resolves the current fixture GDTF path from its edit control or table row, and `TrussEditDialog` still resolves the current truss GDTF path through the scene truss resource reference. Metadata loading remains in `core/` through `LoadGdtfMetadataSummary(...)`; the reusable panel receives an already loaded `GdtfMetadataSummary` or an unavailable-state command.

Fixture Edit and Truss Edit now share one metadata presentation component. Their Apply/OK behavior, physical-property mutation, derivative creation, truss GDTF generation, table writes, undo state, previews, viewer refresh, and hoist/load recalculation behavior are unchanged. The next approved reusable panel extraction is `GdtfPhysicalPropertiesPanel`.

## Checkpoint 05 completion: reusable editor panels

Checkpoint 05A introduced the reusable read-only `GdtfMetadataPanel`. Checkpoint 05B, 05C, and 05D complete the remaining reusable GDTF panel extraction for the current Fixture Edit and Truss Edit dialogs.

- `GdtfPhysicalPropertiesPanel` presents typed physical/type fields using `GdtfPhysicalPropertyField` values for power consumption, weight, length, width, height, and cross section.
- `GdtfTypeIdentityPanel` presents typed identity/source fields using `GdtfTypeIdentityField` values for fixture type name, manufacturer, model name, and source file reference.
- `GdtfModesPanel` presents ordered mode selection, derived channel count, and read-only channel rows. It keeps exact mode strings and exposes only user mode-selection callbacks.

Programmatic setters and panel configuration are non-notifying so dialog initialization, metadata refresh, source browsing updates, and post-apply refreshes do not create false modifications. User edits are reported through typed field callbacks and are translated by the host dialogs to the existing fixture or truss column semantics.

Fixture Edit now composes metadata, type/source, physical properties, and modes as reusable GDTF panels. Category, visual color, MVR color, patch, transform, layer, hang position, fixture ID, and instance name remain host/project fields. Fixture physical writes, derivative creation, revision audit, shared type propagation, mode application, table resync, scene updates, hoist recalculation, and viewer refresh remain host-owned.

Truss Edit now composes metadata, manufacturer/model identity, and physical/type fields as reusable GDTF panels. MVR instance fields remain outside common GDTF panels. Truss generation, table and scene update ordering, undo behavior, generated source/model updates, metadata refresh, preview refresh, and viewer refresh remain host-owned.

Checkpoint 05 is complete after these reusable panels. Checkpoint 06 is next: compose the reusable panels into `GdtfEditorPanel` without introducing session/apply adapters yet.

## Checkpoint 06 reusable editor panel composition

Checkpoint 05 is complete and merged. Checkpoint 06 introduces `GdtfEditorPanel` as a reusable wxWidgets composite under `gui/gdtf/` that owns and arranges exactly one `GdtfMetadataPanel`, `GdtfTypeIdentityPanel`, `GdtfPhysicalPropertiesPanel`, and `GdtfModesPanel`.

The composite remains presentation-only. It accepts typed section configuration for visibility, titles, expanded behavior, and single-column or balanced two-column layouts. The stable single-column order is metadata, type identity, physical properties, then modes and channels. The stable two-column arrangement places metadata and type identity on the left, with physical properties and modes and channels on the right. Hidden sections are skipped by layout insertion while their child panel instances remain reusable if shown again.

`GdtfEditorPanelPresentation` aggregates the existing child panel presentation types and keeps metadata availability explicit. Programmatic setters forward to the child panels and must not emit user-change callbacks. `SetUnavailable()` deterministically clears metadata, identity fields, physical fields, mode selection, channel count, and channel rows through the existing child panel contracts.

The panel performs no file loading, archive reading, metadata discovery, GDTF mutation, derivative creation, project apply behavior, undo/redo, viewer refresh, or hoist-load recalculation. Fixture Edit and Truss Edit still own their direct child panel instances and all project/apply semantics in this checkpoint.

Checkpoint 07 is next: migrate `FixtureEditDialog` and `TrussEditDialog` from direct child-panel ownership to `GdtfEditorPanel` while preserving host-owned project, apply, mutation, scene update, and recalculation behavior.

## Checkpoint 07 host composition migration

Checkpoint 07 is complete. `FixtureEditDialog` and `TrussEditDialog` now instantiate one `GdtfEditorPanel` each instead of directly owning `GdtfMetadataPanel`, `GdtfTypeIdentityPanel`, `GdtfPhysicalPropertiesPanel`, or `GdtfModesPanel` instances.

Fixture Edit uses the composite with metadata, fixture type/source identity, physical power/weight, and modes/channels visible. Truss Edit uses the same composite with metadata, truss manufacturer/model identity, dimensions/weight/cross-section physical fields, and the modes section hidden. The composite gained only narrow presentation-only forwarding for incremental modes updates used by the fixture host.

The composite remains presentation-only. It has no project, table, MVR, loader, mutation, derivative, truss-generation, undo, viewer, or `GdtfEditSession` responsibilities. Host dialogs still translate typed callbacks into their existing modified-column flags and continue to own Apply/OK/Cancel behavior, table updates, scene updates, previews, metadata loading, fixture derivative handling, truss generation, undo, and viewer refresh.

This checkpoint does not implement direct `.gdtf` startup routing, standalone GDTF windows, direct edit-session binding, or `GdtfApplyRequest` adapters. Checkpoint 08 remains the next controlled architecture step.

## Checkpoint 08A audit: host-owned sessions, legacy-owned writes

Checkpoint 08A is complete from an architecture-boundary perspective. `FixtureEditDialog` and `TrussEditDialog` each own one `gdtf::GdtfEditSession` and construct it with `BuildProjectFixtureGdtfEditSession(...)` or `BuildProjectTrussGdtfEditSession(...)` from stable row UUID data, resolved source path, shared `LoadGdtfDocument(...)` output, conservative source kind, and the current descriptive write policy.

`GdtfEditorPanel` remains presentation-only. It has no session member, loader, project context, table access, write policy, Apply logic, viewer dependency, or mutable project access. A small GUI binding helper maps reusable panel field enums to `gdtf::GdtfFieldId`, extracts session presentation text, and applies field capability editability without owning project/write behavior.

Session state is authoritative for supported Checkpoint 08A fields. Host callbacks call `SetValue(...)`, validation is evaluated before legacy mutation, rejected numeric text is tracked outside the session as pending host validation, and reversible dirty tracking mirrors session field dirtiness into the existing compatibility flags. MVR/project-only fields continue to use the existing host tracking.

The existing Apply implementations remain responsible for all side effects. They may consume session-backed values through the current panel getters, but derivative creation, GDTF physical-property mutation, truss generation, table resync, scene updates, undo, project dirty state, previews, hoist/load recalculation, and Viewer2D/Viewer3D refresh are intentionally not moved to adapters in this checkpoint. No `GdtfApplyResult`-driven Apply path was introduced.

Checkpoint 08B should migrate Fixture Apply behavior to a dedicated adapter. Checkpoint 08C should migrate Truss Apply behavior to a dedicated adapter.

### Checkpoint 08A fixture source-resolution correction

The Checkpoint 08A binding now preserves the distinction between portable fixture source references and resolved operational paths. Fixture sessions use an explicit editor source reference supplied by the host, and Fixture Edit resolves project-relative sources through the existing deterministic resolver before calling shared document readers or legacy preview/mode/metadata services. The composite panel remains presentation-only and is no longer an authority for GDTF file I/O paths.

## Checkpoint 08B audit update

The Project Fixture apply path now has a dedicated non-GUI adapter in `core/gdtf/editor/project_fixture_gdtf_apply_adapter.{h,cpp}`. Its request boundary is `GdtfApplyRequest`, extended with stable host identity and source classification so adapters do not infer fixture identity from table rows. Its result boundary is `ProjectFixtureGdtfApplyResult`, which embeds `GdtfApplyResult` and reports resulting type, mode, source reference, resolved path, physical values, affected fixture UUIDs, changed weight positions, derivative/write status, and refresh flags.

The adapter enforces write policy before project mutation: read-only document edits fail, owned-file overwrites require an explicit overwrite policy, derivative-required edits use an injected derivative service before Weight/Power mutation, and unsupported policies fail safely. Physical-property propagation is matched by the resulting source/type family and uses stable fixture UUIDs; mode selection remains fixture-local and is not propagated. UI, viewer, hoist, undo presentation, and error dialogs remain host-owned. Truss Apply is intentionally unchanged and remains the next checkpoint.

## Checkpoint 08C audit update

The prior Checkpoint 08B merge was incomplete because FixtureEditDialog still performed the legacy fixture GDTF apply flow directly and the guard could pass by finding the adapter token in its own implementation file. The guard now inspects FixtureEditDialog itself for adapter construction, BuildApplyRequest(), result success handling, and absence of migrated legacy operations. Checkpoint 08C adds ProjectTrussGdtfApplyAdapter for controlled non-GUI truss generation and leaves StartupRouter/direct .gdtf opening to Checkpoint 09.

## Checkpoint 08D stabilization audit

Checkpoint 08D stabilizes the runtime transaction order for Fixture and Truss Edit. Tables are no longer used as pre-commit transaction buffers before adapter success, Fixture adapter results carry prepared fixture mutations instead of committing the live collection, and hosts reconcile row caches and sessions to the actual resulting GDTF source. Checkpoint 08 is therefore stabilized; Checkpoint 08E is the next UI-only refinement, while startup routing remains after stabilization and UI review.

## Checkpoint 08E1 layout refinement audit

Checkpoint 08E1 is visual and compositional only. Fixture Edit now has a compact scrollable Fixture instance pane, a primary GDTF workspace, and a native tabbed visual-resource pane. Truss Edit now has a compact scrollable MVR instance pane and a right workspace split between a taller 3D preview and the GDTF editor.

`GdtfEditorPanel` keeps the reusable child-panel ownership boundary from Checkpoint 07: it still constructs one metadata panel, one type identity panel, one physical properties panel, and one modes panel. The composite now uses typed section placements and a native splitter for overview/workspace arrangements. Fixture orders Type, Metadata, Physical in the overview and Modes in the workspace. Truss orders Type and Metadata in the overview and Physical properties in the workspace.

Flat section containers replace the previous heavy nested static-box sections. The reusable panel remains presentation-only and has no `ConfigManager`, project, session, adapter, loader, viewer, undo, or table dependency. Layout persistence remains host-owned through the narrow GUI layout-preferences helper.

The current mode/channel text representation remains intentionally unchanged in 08E1. The dedicated Modes and Channels pane is the documented extension point for Checkpoint 08E2, which will introduce the hierarchical Mode and Channel Browser.


Checkpoint 08E1 visual follow-up keeps the two-pane editor architecture but increases gutter spacing between GDTF columns. Fixture visual resources now use two tabs: Preview combines the 3D preview above the fixture image, and Symbols shows an official root-level GDTF SVG thumbnail resource above the three Perastage-generated Top, Front, and Side symbol views when the archive exposes that SVG through the `FixtureType` `Thumbnail` resource.


Checkpoint 08E1 truss follow-up changes the right-side Truss workspace into two GDTF columns: the second column contains only Truss type and GDTF metadata, and the third column contains the host-owned 3D preview above the reusable Physical properties section. The reusable composite exposes only a generic workspace header host for this preview and still does not expose child GDTF panels to the dialogs.
