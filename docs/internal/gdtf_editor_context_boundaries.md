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
