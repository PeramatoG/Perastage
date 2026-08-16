# Perastage Canonical MVR contract

Perastage Canonical MVR is the standards-valid MVR 1.6 representation of the
current domain scene. Project `scene.mvr`, MVR-xchange, and read-only consumers
use `CanonicalMvrExportOptions()` and the same serializer. The canonical truss
representation is the standard MVR Symbol/Symdef representation. A standalone
export also uses this policy when the user selects **Standard MVR
representation**.

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
Export MVR resolves the persisted standalone preference at the application
boundary and passes explicit options to the same exporter; Standard therefore
matches canonical geometry policy, while Direct Geometry3D derives only the
standalone compatibility representation. Direct domain serialization and
Inspector snapshots remain independent of GUI preferences and operate on the
scene supplied by their caller.

The `.pstg` container continues to own application and workspace state outside
`scene.mvr`: window perspectives, splitter positions, selections, preferences,
toolbar state, caches, revision/dirty bookkeeping, and machine-specific paths.
Only portable state that describes or supplements the scene belongs in MVR.
