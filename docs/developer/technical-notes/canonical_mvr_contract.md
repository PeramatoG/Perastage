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
isolates malformed payloads as inert recovery evidence, omits anything claiming
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
Layer merge equality includes the authored matrix values. Publication projects
non-elevation layers to elevation while retaining represented world placements
and rebasing retained source children. Unrepresentable source values are disclosed.

Fixture, Support and Truss each own one optional standard Addresses collection with ordered DMX Address
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
Publication validates each Network independently against the effective definition.
An invalid or unresolved assignment is omitted with its exact values in a warning;
independent DMX Address entries survive. Optional unusable Support/Truss bindings
are omitted rather than given a fictitious mode. Required Fixture bindings use
the caller's existing installed fallback profile and an actual declared mode when
needed, with an explicit substitution warning. No GDTF file or mode is fabricated.
Controlled rebind commands remain strict and atomic before editor mutation.
Standard addressed Truss resources remain byte-preserved unless a validated
canonical derivative is issued: final verification checks both source/output
SHA-256 digests, the exact paths and actual WiringObjects/mode relationships.
Type metadata edits are retained in explicit existing TrussInfo overrides; they
never silently rewrite the authored standard GDTF. A unique legacy-native filename alias may be resolved in the scene resource
directory before selecting a fallback. Imported source owners and explicit failed
acquisition evidence never use that alias policy. Missing model/dependency
references are omitted individually, preserving owners, placements and a valid
empty Geometries container without invented replacement geometry.
SceneObject-to-Truss conversion copies
compatible identifiers, Classing and matrix provenance; retained source issues,
FocusPoint, multiple models, Symbol ownership and nonidentity geometry cause
atomic structured refusal. SceneObject Addresses, including empty presence, are retained by the validated
source-field projection when their standard structure is valid.
Fixture-to-Support conversion preserves the complete collection, Classing,
standard identifiers (including CustomId/CustomIdType presence and explicit
zero), GDTF binding, position,
layer/group ownership and numeric matrix source. Legacy primary patch strings
are materialized through the existing patch policy. Focus, Fixture Color,
Pan/Tilt inversion, explicit project color/symbol/power or unresolved physical
property overrides and retained
source conflicts or missing required GDTF mode/binding return a structured
field-specific refusal before mutation or
Undo publication. Unresolved scene preservation evidence also blocks conversion:
its source UUIDs survive merge remapping and may contain opaque references. Derived fixture metadata is not a new Support standard field;
existing hoist load/function defaults remain intentional conversion semantics.
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
identity/resource/mutation services. Symbol-based focus geometry retains standard Symbol UUIDs and references through
the source-structure projection described below. A required Geometries
container may be empty; no replacement mesh is invented for an empty focus.

Export preparation repairs a private snapshot before validating the actual
projection. Recoverable source defects produce precise warnings after a real
repair, omission or substitution; the validator still rejects invalid generated
output. PSTG and MVR-xchange use that same preparation and serializer. Successful
publication is not proof that every provider extension survives.
See the [coverage audit](../../audits/mvr-1.6-data-integrity-and-roundtrip.md)
for protected structures, unresolved P0 losses and the official XSD/UserData
discrepancy. Full-document XSD conformance must not be inferred from the internal
structural validator or from successful export/reimport.


Classes are a persistent scene-wide UUID registry, independent of layers. Each
supported Fixture, GroupObject, FocusPoint, SceneObject, Truss and Support owns
its optional direct Classing UUID. Export retains unused definitions and exact
optional names; equal names never identify or coalesce classes. Merge remaps
real identity collisions and incoming references together, including cross-kind
collisions regardless of object replacement policy. Malformed/duplicate source Class identities receive deterministic occurrence
identities; uniquely resolvable Classing targets are updated together. Genuinely
ambiguous/unresolved Classing relationships are omitted with a precise warning,
without discarding unrelated definitions or objects. A final projection check
continues to reject accidental serializer omissions.

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

Canonicalization operates on a private export snapshot. Recoverable import
anomalies must be repaired or projected with precise warnings rather than
turning semantic preservation checks into a general save veto. The implemented
rules are deliberately narrow:

| Input | Canonical action | Diagnostic |
| --- | --- | --- |
| Repeated modeled source UUID on import | Apply the existing occurrence-level resolver result to typed objects and retained XML before Classing/topology capture | Import log records occurrence, old/new UUID and placement |
| Earlier-session repeated SceneObject source owners | Reconcile with actual imported identities using the existing resolver or unique retained local-matrix provenance | Warning / IdentityChanged; resources and Class targets unchanged |
| Absent or XML-whitespace-only optional GDTF pair | Omit the empty optional pair | Equivalent normalization |
| Incomplete substantive Truss/Support optional binding | Omit the binding and dependent Network assignments, retaining DMX Address entries, geometry and other object data | Warning / DataOmitted with values and owner/field |
| Complete substantive optional binding | Preserve authored Spec/Mode and resource relationships | Existing resource/reference validation |
| Archive/serialization/publication failure | Fail truthfully; retain previous destination and dirty state | Error |

The official [1.5 text](https://github.com/mvrdevelopment/spec/blob/1ea072e8581f82f78a2e18ca429d893a10ea7afd/mvr-spec.md#node-definition-truss)
lists Truss GDTFSpec as optional and GDTFMode as required; the
[1.6 Table 28](https://github.com/mvrdevelopment/spec/blob/098d3791f77f0895bd859adf01864b4826e2006f/mvr-spec.md#table-28--truss-node-childs)
makes both optional, with Mode mandatory when a substantive Spec is defined.
The reader accepts older empty pairs permissively and emits the supported 1.6
representation; the input fixtures are checked against the pinned 1.6 XSD, not
claimed to satisfy a separate historical 1.5 schema.

No GDTF mode, definition, reference target or geometry is invented by these
rules. Newly imported topology uses the same per-XML-node identity decisions
as the typed reader. Earlier-session recovery never allocates another editable
identity and never rewrites model filenames based on UUID substrings. The
imported archive remains byte-identical. Export warnings use the existing
structured diagnostic/log and MVR result dialog; no ignore-preservation option
or alternate PSTG scene serializer is added.

The publication repair service distinguishes equivalent normalization, identity
repair, specific omission/substitution and fatal technical failure. It does not
change all Error severities to Warning. Only the repaired snapshot is serialized;
strict pure validators still expose defects on the unmodified imported scene.

| Additional recoverable case | Published result |
| --- | --- |
| Valid integer beyond native editor range | Preserve lexical standard integer; explicit native edits win |
| Invalid scalar/source-only field | Publish the represented editable value; identify exact original field/value |
| Missing geometry/media or invalid relationship | Omit only that reference/payload; retain independently valid objects/resources |
| Repeated Symbols, missing/cyclic Symdef edge | Repair occurrence identities or remove only the unsatisfiable edge |
| Nonfinite numeric component or broken ownership | Substitute only unrepresentable component or detach invalid parent; retain finite world placement |
| Unknown top-level GDTF FixtureType payload | Explicit canonical projection retains standard fields/resources; removed original XML is disclosed |
| Physical power change without a standard write owner | Keep authored profile and explicit per-fixture planning metadata; warn that external physical patch was not performed |

Material findings are stored as inert escaped text in permitted root
`Data provider="org.perastage.canonical-recovery" ver="1"`. Reopen retains and
logs them; known extraction-directory prefixes become relative resource references
in stored disclosures while local diagnostics retain full context. Evidence
is never executed, reconciled automatically or treated as a
second editable scene. Missing resource bytes cannot be reconstructed from a
warning: the original imported package remains the recovery source. Existing
application-owned planning metadata preserves native power/weight values and
TrussInfo explicitly distinguishes type overrides from GDTF source authority.
Canonical derivative revisions omit the optional generated Date, retaining all
authored revision dates, so wall-clock time does not change canonical payloads.

Coverage remains bounded by executed synthetic/regression cases. New unknown
provider structures require actual projection tests; there is no catch-all
ignore-preservation flag or promise that a corrupt/unwritable archive succeeds.

Valid source-only fields of Fixture, Truss, Support and SceneObject have a
focused export projection: the retained XML is validated against its existing
MVR 1.6 type, attached to the matching generated UUID owner and written in
standard child order. This includes CastShadow, protocols, commands, alignments,
connections, mappings and other fields listed in `mvr_standard_field_projection.cpp`.
MappingDefinition is retained under AUXData. Source references must resolve;
source resources use the normal resource collection. Merge remaps retained owner
UUIDs and standard references, including equivalent uppercase/lowercase UUID
spellings, along with resource paths. No source fragments are
executed or injected into the editable scene, and no schema extensions are added.
Deletion of a modeled owner deletes its retained fields. An editable/source
field conflict is diagnosed rather than choosing a value silently. Reopening
reconstructs the evidence from the canonical scene.mvr, so later saves and
standalone canonical exports follow exactly the same policy.

Save preparation rolls back scene, layouts, resources, preferences and dirty
state on failure. Archive publication closes the temporary file then replaces
the target atomically on the same filesystem; there is no overwrite-by-copy
fallback. Save As commits its session path only on confirmed success. A failed
save exposes all export findings in the dialog's expandable details after the
busy overlay has closed.

## Retained standard scene structure

`mvr_source_structure_*` owns a versioned, immutable scene-local provenance value.
It retains shallow standard node payloads, authored local/world matrices and
editable display baselines. VideoScreen/Projector payloads, media, multipatch,
parametric ChildList ownership and Symbol geometry are validated against the
existing standard types. They are restored around the ordinary typed writers'
edited nodes. This is neither a second Fixture serializer nor a new PSTG schema;
reopening reconstructs provenance from the canonical scene.mvr. Scene history
copies share the immutable evidence. No imported MVR is modified.

Nested Symdef instances have one typed definition/instance registry. Display
expansion does not replace canonical references. Canonical export preserves
unused definitions, instance UUIDs, class links and placement. Model/geometry
edits use an isolated standard definition for the changed instance, retaining
original definitions. The export snapshot owns these changes; the live scene
is unchanged. Explicit standalone direct-geometry compatibility is applied
without subsequently restoring the Truss Symbol representation.

Models with external dependencies retain their original bytes and relative
resource tree under a stable package namespace. The resource collector owns
packaging, collisions, transitive closure and missing-resource diagnostics;
merge uses the same closure. Acquired package resources cannot escape their
package root. No resource bytes are executed and no official schema is changed.

Normal PSTG save, standalone canonical export and MVR-xchange all use these
same owners. Valid retained structures no longer require a warning/consent
loop merely because they have no editor. Unhandled generated-output invalidity and actual archive IO failures remain
specific failures, never a successful corrupt or unwritten archive. Coverage remains
bounded by the pinned standard and the regression fixtures; arbitrary provider
extensions and every possible third-party package are not certified.

The external-scene CLI reports successful canonical recovery warnings in its JSON
`diagnostics` array and on stderr for human output, after atomic publication.
Resource aliases that identify the same physical file are equivalent, including
Windows short paths and macOS temporary-directory aliases. Stored recovery
evidence strips both lexical and physical source-root prefixes; local diagnostics
retain their full context. UserData consumers identify providers by name rather
than their position.
