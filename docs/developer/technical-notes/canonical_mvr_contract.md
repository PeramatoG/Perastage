# Perastage Canonical MVR contract

Perastage Canonical MVR is the standards-valid MVR 1.6 representation of the
current domain scene. Project `scene.mvr`, MVR-xchange, and read-only consumers
use `CanonicalMvrExportOptions()` and the same serializer. The canonical truss
representation is the standard MVR Symbol/Symdef representation. Every normal standalone
export uses this policy, independently of saved compatibility preferences.

For an explicit **Direct Geometry3D for truss symbols** standalone export, the
same serializer derives a compatibility representation from a private copy of
the canonical scene. It expands supported Symbol/Symdef truss geometry into
direct `Geometry3D` references without changing the live scene. This is an
alternative standards-valid MVR 1.6 serialization: a Truss requires
`Geometries`, whose graphical children may be either `Symbol` or `Geometry3D`.
The option improves interoperability with applications whose Symbol/Symdef
support is incomplete, but it is never project persistence, canonical snapshot,
MVR-xchange, Inspector, or internal scene authority.

The canonical root contains at most one `GeneralSceneDescription/UserData`.
That element contains one `Data provider="Perastage" ver="1.0"` block. Its
versioned maps supplement scene fidelity, including fixture type information,
`ProjectFixtureMetadataMap`, truss and hoist information, primitive geometry,
and layer appearance. These maps never replace an official MVR or GDTF field
and never contain local absolute paths. Supported Perastage fixture metadata is
applied by project restore, replace/open, and merge imports. Entries are keyed
to fixture UUIDs; the existing merge remapping then carries each imported
fixture and its restored values to its collision-free identity without changing
unrelated target objects. Unsupported versions and malformed entries remain
non-fatal diagnostics.

Valid foreign root `Data` blocks are stored separately as opaque scene data.
Perastage does not interpret them. Canonical export reparses each stored block,
diagnoses and rejects malformed or misplaced payloads, omits anything claiming
Perastage ownership, deduplicates repeated payloads, and places accepted blocks
beside the single Perastage block under the one legal root `UserData`. Merge
unions these blocks without accumulating duplicates. XML subtree serialization
may normalize insignificant formatting, but preserves provider identity and XML
structure.

`MvrExporter::ExportCanonicalSnapshotToBuffer` copies the supplied `MvrScene`
and performs identity and transform recovery only on that copy. Snapshot and
normal serialization therefore cannot mark the live project dirty or leak
repairs into it. Resource leases are shared by the copy, avoiding duplication
of resource payloads while keeping their lifetime safe.

Interactive project save and standalone export both call `SyncSceneData()` as
an explicit command-boundary preparation step. This commits pending derived
fixture type colors to the domain scene without reintroducing table-wide GUI
resynchronization. Project save then requests the canonical policy. File >
Export MVR starts with **Use compatibility export for this file** unchecked.
The saved preference suggests the representation only after that explicit opt-in;
it remains stored under its existing key. The dialog shows the transformation
and passes explicit options to the same exporter. Cancelling or unavailable dialog
customization leaves the canonical policy in effect. Direct Geometry3D derives
only the standalone compatibility representation. Direct domain serialization and
Inspector snapshots remain independent of GUI preferences and operate on the
scene supplied by their caller.

The `.pstg` container continues to own application and workspace state outside
`scene.mvr`: window perspectives, splitter positions, selections, preferences,
toolbar state, caches, revision/dirty bookkeeping, and machine-specific paths.
Only portable state that describes or supplements the scene belongs in MVR.

Layer UUIDs and object `layerUuid` ownership are authoritative. Names, including
empty and duplicate names, are preserved independently; generated GUI labels
never replace authored MVR names. The default layer has a canonical deterministic
UUID before export. See [Layer identity](../layer_identity.md) for default policy,
legacy `layer_default` migration, workspace state, and compatibility boundaries.

Layer owns its MVR 1.6 vertical elevation and authored matrix precision. Scene
node transforms are world-space; exported local matrices are relative to the
immediate GroupObject or, for roots, the owning Layer. Import composes each
parent once. Export preserves unedited numeric source values, repairs locals on
a private snapshot after world-space edits, and may omit an identity Layer Matrix.
Layer merge equality includes the authored matrix values. Non-elevation,
malformed or ambiguous layer matrices remain fail-closed.

Fixture owns one optional standard Addresses collection with ordered DMX Address
and Network lists (Address children precede Network), retaining container
presence, optional break attributes and authored values. Missing break means 0;
break IDs identify assignments and never universes. Fixture.address is the
compatibility projection of break 0 only. Unedited nonzero-only or out-of-editor-
range assignments remain preservable without an invented primary assignment.
Core `perastage::patch::SetFixturePrimaryPatch` edits only break 0. Explicit patching
of an absent/empty list creates it; clearing removes only it and retains the
container/other entries. A missing primary in a nonempty list, duplicate breaks
or invalid values produces a structured conflict. Existing table/auto-patch
workflows use that policy; auto-patch publication is atomic. Pending legacy
scalar edits reconcile only on a private export snapshot with an unambiguous
mapping. A final serialized projection check protects the complete ordered list.
Fixture Network retains required case-sensitive GDTF wire-geometry names and
optional ipv4/subnetmask/ipv6/dhcp/hostname presence and text. Patch edits never
modify this list. Names are not UUID references and are not remapped at merge.
Known-invalid or ambiguous GDTF references block publication; unavailable local
GDTF evidence retains metadata without guessing and reports ResourceMissing at
publication, preventing a dummy GDTF from invalidating wire references. Core rebind validation rejects unsafe definition changes before
publishing. Malformed/unknown data, other address consumers and unsupported
extensions remain guarded. Network geometry/hostname control whitespace also
fails closed because the XML writer cannot retain its character-reference form.
No network settings are applied.
Published Universe.Address strings and signed break IDs are preserved even where
the unchanged pinned XSD's integer/nonnegative restrictions disagree.

Root `UserData` precedes `Scene`, as required by the official XSD. Every exported
GroupObject retains `ChildList`, including empty groups, to satisfy the pinned
XSD; published MVR 1.6 Table 22 permits its absence. These
representation rules preserve standards-valid identity round-trips.

FocusPoint uses the existing geometric SceneObject value with a distinct
`SceneObjectKind::FocusPoint` and is serialized with its standard element name.
Direct geometry, ownership, transforms and Fixture Focus links share the existing
identity/resource/mutation services. Symbol-based focus geometry still requires
preservation support and blocks writes. A required Geometries
container may be empty; no replacement mesh is invented for an empty focus.

Export preparation rejects durable known-loss evidence before identity/transform
recovery or archive writing. PSTG and MVR-xchange snapshots use that same gate.
This is bounded protection, not proof that every standard field survives.
See the [coverage audit](../../audits/mvr-1.6-data-integrity-and-roundtrip.md)
for protected structures, unresolved P0 losses and the official XSD/UserData
discrepancy. Full-document XSD conformance must not be inferred from the internal
structural validator or from successful export/reimport.


Classes are a persistent scene-wide UUID registry, independent of layers. Each
supported Fixture, GroupObject, FocusPoint, SceneObject, Truss and Support owns
its optional direct Classing UUID. Export retains unused definitions and exact
optional names; equal names never identify or coalesce classes. Merge remaps
real identity collisions and incoming references together, including cross-kind
collisions regardless of object replacement policy. Malformed/ambiguous source
identities, dangling references or unrepresented class content remain blocked;
a final projection check rejects serializer omissions before publication.

This preserves the identity model needed for future class visibility filtering
across layers in Viewer2D, Viewer3D and Layout Mode. There are no visibility
controls or flags in this change. Visibility presets and documentation settings
will belong to PSTG, without adding proprietary Class attributes to MVR.


## Preservation, canonicalization and controlled repair

Preservation protects valid authored information and meaning, including fields
without an editor. Canonicalization may change representation: standard element
ordering, equivalent numeric spelling, resource packaging with consistent
reference rewriting, identity layer matrices, empty optional assignment
containers, and required XSD containers. It does not authorize changing arbitrary
text, precision, identities or relationships. XML whitespace in otherwise empty
optional assignment containers, including character references, is insignificant; attributes,
child elements and non-whitespace text remain protected. This is a bounded rule,
not permission to discard unknown fields.

For the same scene and effective canonical options, PSTG scene.mvr and standalone
canonical output must have exactly the same ordered ZIP entry names and
uncompressed bytes, including XML text, attributes, values and element order.
ZIP metadata and compression may differ. This compares two generated outputs,
not generated output with imported XML. Project documentation, styles,
visibility presets and preferences belong outside scene.mvr.

FixtureID and FixtureIDNumeric are selection labels, independent of persistent
UUID identity. Authored duplicates, zero, empty strings and optional presence
retain the bounded #2484 policy. Future allocation/duplicate repair must be an
explicit reusable plan, with diagnostics and Undo/Redo when applied to the live
scene. Import repair changes state at an explicit command boundary; canonical
export only derives a private snapshot; compatibility export is a named alternate
operation. None may consume future repair preferences implicitly.

UUID spelling/unique-alias recovery on a private export snapshot remains
available and diagnosed. An ambiguous alias reported by identity recovery now
returns IdentityConflict / Error / ExportFailed before transform preparation or
serialization. No guessed or cleared reference is published. The lower-level
recovery utility still exposes its historical recovery result and is not itself
a general validation API. Generic source UUID/reference coverage, Position
recovery and opaque provider references remain P0; this gate is not proof of
complete reference preservation.

Safe representable content should save without a consent loop. Genuine unsupported
standard content still blocks both serializers until an actual preservation
owner exists. This is an unresolved workflow constraint, not the final product
policy. No validation bypass, alternate project scene or lossy-save override is
provided. The audit records the smallest proposed recovery design and acceptance
conditions separately from implemented behavior.
