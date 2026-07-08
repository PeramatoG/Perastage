# GDTF editor context boundaries

Checkpoint 03 adds non-GUI editing-domain models for future reusable GDTF editing. It does not add a new editor window, reusable wx panel, startup route, file association, visual simulation feature, GDTF writer, or MVR writer.

## Document model

`gdtf::GdtfDocument` wraps the existing read-only archive and description services. It exposes the source path, archive diagnostics, description diagnostics, identity metadata, DMX mode names, repeated-family summaries, source-file presence, and validity. It intentionally does not expose project rows, UUIDs, table indices, wxWidgets controls, `ConfigManager`, renderer state, mutable XML nodes, or MVR write state.

Repeated or future GDTF families are represented as collections. Wheels remain a collection in the shared read snapshot and are summarized as a repeated family instead of being promoted to singleton editor fields.

## Editable value model

`gdtf::GdtfEditableValues` stores only candidate GDTF type/editor values used by a future common editor:

- fixture type name;
- manufacturer;
- model/name presentation value;
- mode name when the host treats mode selection as editor state;
- weight;
- power consumption;
- truss length, width, and height;
- truss cross section;
- source file reference as a context-owned presentation adapter.

It does not store project row indices, GUI controls, fixture positions, DMX addresses, fixture visual colors, MVR fixture colors, calculated channel count, or calculated truss load.

## Edit session

`gdtf::GdtfEditSession` owns a context, initial editable values, current editable values, validation diagnostics, and field-level dirty tracking. It distinguishes user edits from derived values by refusing independently editable writes to fields registered as derived/read-only. It never mutates a project, writes a `.gdtf`, refreshes viewers, recalculates hoists, opens dialogs, or reads global configuration.

## Source kind and write policy

Source classification and write policy are separate enums:

- `GdtfSourceKind` covers standalone external files, Perastage fixture library files, Perastage truss library files, GDTF extracted or embedded from MVR, Perastage-generated derivatives, temporary/generated files, and unknown sources.
- `GdtfWritePolicy` covers read-only, overwrite owned file, create derivative before mutation, save as a new standalone file, project-controlled generation, and unsupported/not-yet-available.

The session receives both values from its host context and does not infer policy from GUI state.

## Context kinds and adapters

`gdtf::GdtfEditorContextKind` defines `StandaloneFile`, `ProjectFixture`, `ProjectTruss`, and `FutureProjectObject`.

- The standalone factory opens through shared read services, creates a read-only context by default, has no project/MVR dependency, creates no windows, registers no file associations, and writes nothing.
- The project fixture adapter accepts stable fixture data and a resolved source path, builds a session from current model values, records the fixture UUID as host identity, and does not mutate the scene during construction.
- The project truss adapter accepts stable truss data and a resolved source path, records the truss UUID as host identity, represents Perastage truss generation as project-controlled write policy, and does not generate or rewrite a GDTF during construction.

Existing `FixtureEditDialog` and `TrussEditDialog` remain unmigrated in this checkpoint. Their current apply paths, preview wiring, table updates, physical-property write path, truss builder calls, dirty state updates, viewer refreshes, and hoist recalculation prompts remain owned by the existing dialogs and table panels until the next checkpoint.

## Apply request/result boundary

`GdtfApplyRequest` carries the context kind, source path, write policy, current values, and changed field IDs. `GdtfApplyResult` can independently report validation errors, changed GDTF fields, resulting GDTF path/reference, derivative creation, project instance resynchronization, viewer refresh, hoist/load recalculation, project dirty state, and diagnostics. It is intentionally GUI-independent and does not implement physical writes in this checkpoint.

## Field ownership classification

The authoritative code registry is `core/gdtf/editor/gdtf_field_registry.*`.

### Current Edit Fixture fields

| Field | Current source of truth | Ownership decision |
| --- | --- | --- |
| Fixture ID | MVR/project fixture instance | MVR/project instance-level |
| Name | MVR/project fixture instance | MVR/project instance-level |
| Type | Fixture type/name mirrored from GDTF/dictionary/project row | GDTF type-level for common editor purposes |
| Layer | Project layer assignment | MVR/project instance-level |
| Hang position | Project/MVR position reference | MVR/project instance-level |
| Universe | Project patch data | MVR/project instance-level |
| Channel | Project patch data | MVR/project instance-level |
| Mode | GDTF mode name selected for the project fixture | Context-specific |
| Ch Count | Derived from selected GDTF mode channels | Derived/read-only |
| Model File | Host source/reference selection | Context-specific |
| Position X/Y/Z | Project/MVR transform | MVR/project instance-level |
| Roll/Pitch/Yaw | Project/MVR transform | MVR/project instance-level |
| Power | Existing approved physical-property mutation path writes GDTF `PowerConsumption` when allowed | GDTF type-level |
| Weight | Existing approved physical-property mutation path writes GDTF `Weight` when allowed | GDTF type-level |
| Category | Perastage classification/dictionary/project override, not a direct GDTF semantic edit | Project classification/override |
| Visual color | Perastage visual classification propagated by type | Project classification/override |
| MVR color | Official MVR fixture color on the project instance | MVR/project instance-level |

Ambiguous fixture decisions: mode is context-specific because the current dialog selects a project fixture mode from GDTF modes rather than editing the GDTF mode definitions; model file is context-specific because it is a host source/reference selection; category and visual color can look like type metadata in the UI but are Perastage classification values, not direct GDTF semantic fields; channel count is derived and never independently editable.

### Current Edit Truss fields

| Field | Current source of truth | Ownership decision |
| --- | --- | --- |
| Name | MVR/project truss instance | MVR/project instance-level |
| Layer | Project layer assignment | MVR/project instance-level |
| Model File | Host source/reference selection | Context-specific |
| Hang position | Project/MVR position reference | MVR/project instance-level |
| Position X/Y/Z | Project/MVR transform | MVR/project instance-level |
| Roll/Pitch/Yaw | Project/MVR transform | MVR/project instance-level |
| Manufacturer | Truss type metadata used by Perastage truss GDTF generation | GDTF type-level |
| Model | Truss type metadata used by Perastage truss GDTF generation | GDTF type-level |
| Length/Width/Height | Truss type dimensions used by Perastage truss GDTF generation | GDTF type-level |
| Weight | Truss type weight used by Perastage truss GDTF generation | GDTF type-level |
| Load | Calculated/manual project load state | Derived/read-only |
| Cross section | Truss type metadata used by Perastage truss GDTF generation | GDTF type-level |

Ambiguous truss decisions: model file is context-specific because the host may point to a source model archive or an active generated GDTF; load is project load state and must not become GDTF editor state; truss type edits currently cause the existing dialog to call the truss GDTF builder, but the new session only represents that policy and does not generate files.

## Next checkpoint

The next approved checkpoint is reusable panel extraction from the existing `FixtureEditDialog` and `TrussEditDialog`. That work should keep the apply semantics in the existing project hosts while moving reusable field presentation into shared panels.

## Checkpoint 03A capability correction

Checkpoint 03A separates four decisions that were previously represented by one ambiguous editability flag:

- **Field ownership** describes where the value belongs: GDTF type data, MVR/project instance data, derived data, project classification overrides, context-specific host selections, or unsupported future work.
- **Default value kind** in the global descriptor documents the usual field meaning, while the active context capability supplies the effective value kind. The effective kind describes what the edit-session value represents: a GDTF document value, a context selection, a derived read-only presentation value, a host/project value, or an unsupported future value.
- **Host-dialog editability** records whether the current Fixture Edit or Truss Edit dialog can edit the value on its existing project-specific path.
- **Context capability** is the final authority used by `GdtfEditSession` for visibility, editability, and whether a successful change is a document mutation or a context/host selection.

The field registry remains the authoritative classification source. `GdtfEditableValues` now stores only explicitly supported session values and has no generic presentation fallback. This prevents MVR/project fields such as universe, layer, transform, visual color, fixture category, MVR color, truss name, truss load, and channel count from being silently accepted as GDTF session edits.

### Context capabilities after Checkpoint 03A

| Context | Document mutation fields | Context selection fields | Read-only inspection fields | Explicitly excluded from session edits |
| --- | --- | --- | --- | --- |
| Project fixture | Weight, power consumption | Fixture type/name presentation, selected mode, source GDTF reference | Channel count | Fixture ID, instance name, layer, hang position, universe, DMX address, transform, fixture category, visual color, MVR color |
| Project truss | Manufacturer, model, length, width, height, weight, cross section | Source/model reference | Truss load | Truss instance name, layer, hang position, transform |
| Standalone read-only | None | None | Mode and source reference may be exposed for inspection | All edits |
| Standalone editable future policy | Fixture type/name, manufacturer, model, weight, power consumption | None | Mode and source reference remain inspection-only | Project selected mode, source path mutation, MVR/project fields |

Selected mode is a context selection because the current project fixture workflow selects which existing GDTF `DMXMode` the fixture instance uses; it does not edit a mode definition in `description.xml`. Source file reference is also a host selection because it chooses or displays the archive/model reference used by Perastage rather than mutating a standard GDTF document field.

`GdtfApplyRequest` now separates `changedDocumentFields` from `changedContextFields` so future host adapters do not need to infer the distinction from ownership. `GdtfApplyResult` still reports downstream host effects such as derivative creation, project resynchronization, viewer refresh, hoist/load recalculation, and project dirty state independently.

Dirty tracking is data-driven from context capabilities and explicit session storage. Every successful changed `SetValue(...)` call is represented in dirty state, restoring the initial value clears it, and rejected fields leave values, dirty state, and apply requests unchanged.

Existing `FixtureEditDialog` and `TrussEditDialog` runtime paths remain unchanged in this correction. Their Apply/OK behavior, table writes, derivative creation, `SetGdtfProperties`, fixture type propagation, visual color behavior, mode application, truss GDTF generation, metadata display, previews, undo/redo, hoist recalculation, and viewer refresh remain on the existing code paths.

The next approved checkpoint is to extract the first reusable GDTF metadata panel while keeping current host apply semantics intact.

## Downloaded GDTF insertion boundary

Downloaded GDTF insertion now uses `PrepareGdtfFixtureInsertion(...)` before the Add Fixture dialog is shown. The service is non-GUI and read-only: it validates file existence, regular-file status, non-empty size, archive readability, description readability, FixtureType identity, and ordered named DMX modes without mutating the project or rewriting the archive. Compatibility fallbacks for non-canonical `description.xml` locations are logged and allowed only when unambiguous. Ambiguous, unsafe, malformed, or mode-less files fail with diagnostics instead of escaping exceptions or silently returning.

Checkpoint 03A did not originally modify the GDTF Share download or `AddFixtureFromGdtfPath(...)` flow; the insertion failure was in the pre-existing legacy discovery path. The next approved checkpoint remains the first reusable GDTF metadata panel extraction.

## Checkpoint 05A reusable metadata presentation boundary

Checkpoint 05A begins the reusable GUI migration by extracting `GdtfMetadataPanel` from the current Fixture Edit and Truss Edit dialogs. The panel is presentation-only: it owns the metadata labels, read-only multiline description control, unavailable-value fallback, and wrapping/layout behavior. It does not own project rows, table state, `ConfigManager`, scene objects, MVR state, viewer refresh, undo/redo, mutation, derivative creation, truss generation, startup routing, or file associations.

Host dialogs continue to own source-path resolution and call `LoadGdtfMetadataSummary(...)` from `core/`. Fixture Edit and Truss Edit then pass the loaded `GdtfMetadataSummary` into the panel or call `SetUnavailable()` when loading fails. No mutation or apply semantics changed in this checkpoint, and the complete `GdtfEditorPanel` remains out of scope.

The next approved panel extraction is `GdtfPhysicalPropertiesPanel`, keeping the existing project-host apply semantics intact until a later context-adapter migration.
