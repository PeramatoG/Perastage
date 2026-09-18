# GDTF editor architecture

The GDTF editor is split between GUI-independent document and editing services,
reusable presentation panels, and project host dialogs. This separation keeps
GDTF parsing and mutation out of widgets while leaving project, undo, and viewer
side effects with the application workflows that own them.

## Core read and document model

`core/gdtf/` owns archive access and description parsing. Reads are non-mutating:
they locate a safe, unambiguous `description.xml`, preserve diagnostics, and
expose identity, metadata, modes, channels, wheels, resources, and physical
properties without rewriting the source archive.

`core/gdtf/editor/gdtf_document.*` presents that data as `gdtf::GdtfDocument`.
The document contains no wxWidgets controls, project rows, table indices,
renderer state, or mutable XML nodes. Repeated GDTF families remain collections
rather than being flattened into singleton editor fields.

Downloaded-fixture preparation uses the same read boundary. It validates the
archive, fixture identity, and named modes before opening the project insertion
workflow. Non-canonical description locations are tolerated only when
unambiguous; unsafe, ambiguous, malformed, empty, or mode-less input fails with
structured diagnostics.

## Edit sessions and contexts

`GdtfEditSession` owns initial and current editable values, validation results,
field capabilities, and reversible field-level dirty state. The registry in
`core/gdtf/editor/gdtf_field_registry.*` classifies values as GDTF document
fields, host/context selections, derived read-only values, project values, or
unsupported values. A session does not write files or mutate the project.

Contexts supply source kind, write policy, stable host identity, and supported
capabilities:

- `ProjectFixture` exposes fixture type/source/mode selections and supported
  document values such as description, weight, and power consumption.
- `ProjectTruss` exposes source selection and the type data used for GDTF
  generation: manufacturer, model, dimensions, weight, description, and cross
  section.
- `StandaloneFile` is read-only. There is no standalone editor startup route or
  file association.

Mode selection chooses an existing GDTF mode for a project fixture; it does not
edit a `DMXMode`. Source references are host selections, not document fields.
Patch, transforms, layers, position references, fixture IDs, project colors,
category overrides, truss load, and other MVR/project fields stay outside the
GDTF session. Channel count and calculated load remain derived and read-only.

## Apply and mutation boundaries

The project fixture and truss adapters under `core/gdtf/editor/` consume a
session-built request and validate it without depending on GUI code. Apply
requests separate changed document fields from changed context fields.

Fixture document edits use the shared GDTF mutation service. Read-only sources
reject writes; verified Perastage-owned files may be overwritten; ordinary
external, library, or MVR-extracted sources require a stable derivative before
mutation. A combined Apply rewrites `description.xml` at most once and appends
at most one standard GDTF revision. The resulting type/source family controls
physical-property propagation, while selected mode remains fixture-local.

Truss type edits use the project-controlled truss generation path. Geometry-only
and MVR-instance-only edits do not generate a GDTF. Cross-section output follows
the current rules in [GDTF Mutation Policy](../../gdtf_mutation_policy.md).

Adapters prepare results before project commit. Host dialogs create one undo
checkpoint only after successful preparation, then commit table and scene state.
A failed adapter does not commit project, table, or session state. External file
writes are outside project undo and are reported separately. Portable project
references remain distinct from resolved operational filesystem paths.

## GUI composition and host integration

`gui/gdtf/GdtfEditorPanel` composes focused metadata, type identity, physical
properties, modes/channels, and wheel-inspection panels. These components own
controls, presentation state, layout, and typed user callbacks only.
Programmatic presentation updates must not emit user-change callbacks.

`FixtureEditDialog` and `TrussEditDialog` each own a session and one composite
panel. A typed binding maps panel fields to the session. The host dialogs retain
source-path resolution, project-only fields, previews, symbols and thumbnails,
validation messages, undo, table/scene commits, hoist or load recalculation, and
Viewer2D/Viewer3D refresh. The reusable panel does not access project tables,
`ConfigManager`, MVR state, mutation services, or viewers.

Fixture Edit shows metadata, type/source, physical properties, modes/channels,
and wheel inspection alongside its project fields. Truss Edit uses metadata,
type identity, dimensions, weight, and cross-section sections and hides fixture
mode/wheel sections.

## Standards and compatibility ownership

[GDTF read/write compatibility](gdtf_read_write_compatibility.md) owns tolerant
read rules and strict write expectations. [GDTF Mutation Policy](../../gdtf_mutation_policy.md)
owns all intentional archive mutations and revision semantics, while
[GDTF resource identity](../../gdtf_resource_identity.md) owns derivative and
project-reference identity. Mode/channel and wheel interpretation remain in
their focused contracts:

- [Mode and channel browser](gdtf_mode_channel_browser.md)
- [Wheel and attribute inspector](gdtf_wheel_attribute_inspector.md)
- [Editor UI layout](gdtf_editor_ui_layout.md)

Compatibility fallbacks belong in read paths and must not silently change strict
output. The shared Core model remains GUI- and project-independent so Perastage
and other consumers can reuse its standard semantics without sharing host
behavior.

## Current limitations

- Direct standalone `.gdtf` editing and startup routing are not implemented.
- Mode definitions and wheel data are inspected read-only; project mode
  selection does not modify them.
- Tube-specific cross-section height and wall thickness are not editable.
- External file changes cannot participate in project undo.
