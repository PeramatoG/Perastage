# Perastage Canonical MVR contract

Perastage Canonical MVR is the standards-valid MVR 1.6 representation of the
current domain scene. Project `scene.mvr`, ordinary standalone export,
MVR-xchange, and read-only consumers use `CanonicalMvrExportOptions()` and the
same serializer. The canonical truss representation is the standard MVR
symbol/reference representation. `DirectGeometry3DForTrussSymbols` remains an
explicit derived compatibility transformation and is not project persistence
or snapshot authority.

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

The `.pstg` container continues to own application and workspace state outside
`scene.mvr`: window perspectives, splitter positions, selections, preferences,
toolbar state, caches, revision/dirty bookkeeping, and machine-specific paths.
Only portable state that describes or supplements the scene belongs in MVR.
