# MVR exporter diagnostic behavior

MVR export reports `MvrExportDiagnostic` records rather than treating log text as
presentation data. Each record has a stable code, `Info`/`Warning`/`Error`
severity, semantic impact, explicit user-visibility policy, optional object and
resource context, and a technical detail for logging. The model is independent
of wxWidgets.

## Classification policy

The exporter classifies the effect of recovery, not just its original reason.
Canonical UUID spelling changes, inferred layers, safely recovered duplicate
identity fields, transform recalculation that preserves world transforms,
legacy Position remapping, normal pruning, successful GDTF canonicalization,
and requested Symbol/Symdef preservation or flattening are `Info` and log-only.
In particular, uppercase, braced, or otherwise noncanonical spelling represents
the same UUID and is not a user warning.

Meaningful identity replacement, cleared references, duplicate numeric fixture
IDs, fallback or missing GDTFs, omitted resources or textures, placeholder or
empty geometry, rejected foreign metadata, omitted invalid DMX addresses, and
an unavailable requested compatibility representation are user-visible
`Warning` records. Structural validation, transform integrity, canonicalization,
archive I/O, and hierarchy recursion failures are user-visible `Error` records
and make the operation fail.

Identity storage mismatches are evaluated using both sources. A malformed or
missing field remains log-only when the other source preserves the effective
identity; two different valid identities produce one `IdentityConflict`, while
two unusable sources produce one `IdentityGenerated`. Unresolved non-empty
Position references produce `ReferenceCleared` because the reference is omitted.
Failure to create a required physical-property GDTF patch or mandatory
SceneObject placeholder geometry is fatal rather than silently exporting stale
properties or deleting the object.

The stable code policy is:

| Codes | Severity | Normal UI |
|---|---|---|
| `IdentityCanonicalized`, `LayerInferred`, `TransformRepaired`, `InternalRecovery` | Info | Hidden |
| `IdentityGenerated`, `IdentityReassigned`, `IdentityConflict`, `ReferenceCleared`, `SymbolIdentityReplaced`, `FixtureIdReassigned` | Warning | Shown |
| `GdtfFallbackUsed`, `GdtfMissing`, `TrussGdtfMissing`, `GdtfPatchFailed` | Warning | Shown |
| `TextureMissing`, `ResourceMissing`, `ResourceDuplicate`, `SupportGeometryMissing`, `PlaceholderGeometryUsed` | Warning | Shown |
| `CompatibilityRepresentationUnavailable`, `ForeignMetadataDiscarded`, `DmxAddressOmitted` | Warning | Shown |
| `TransformInvalid`, `StructuralValidationFailed`, `CanonicalizationFailed`, `ArchiveIoFailed`, `HierarchyRecursion` | Error | Shown; export fails |

`GetExportDiagnostics()` is the authoritative API. `GetExportWarnings()` is a
temporary compatibility view built from non-Info structured records; callers
must not infer policy by parsing its English text.

## Non-fatal warnings

When `GeneralSceneDescription.xml` references a `fileName`/`GDTFSpec` entry that is missing from the archive source set, or the same archive path appears more than once, export continues and records a warning:

- Missing file warning: `Referenced file '<name>' is missing from the archive and will be omitted.`
- Duplicate file warning: `Referenced file '<name>' appears multiple times; duplicates will be ignored.`

Diagnostics are available through `MvrExporter::GetExportDiagnostics()` after
`ExportToFile(...)`/`ExportToBuffer(...)`.

## Fatal errors

Structural MVR compliance errors still fail export (for example missing
`GeneralSceneDescription`, invalid version, or invalid required IDs). Every
export-abort path retains a structured Error for the caller.

## UI behavior

Standalone GUI export shows exactly one result dialog. Warning summaries are
aggregated by semantic code, while a bounded, resizable details control lists
affected objects and resources. Log-only diagnostics never enter this summary.
The persistent diagnostic log remains the owner of full technical context.
Project persistence, canonical snapshots, and MVR-xchange remain non-modal and
may inspect structured records according to their own policy; they do not log a
second copy of exporter-owned diagnostics.
## MVR node validity notes

Support is a standard MVR scene node and is preserved as `<Support>` during import/export; if geometry is missing, Perastage writes an empty `<Geometries/>` plus required `ChainLength` so the logical Support remains schema-valid without inventing model geometry. Generic `<SceneObject>` nodes require a `<Geometries>` child, so Perastage writes a small 10 cm placeholder cube when no source geometry is available instead of writing invalid XML. Truss children are emitted in XSD `xs:sequence` order, with `Matrix` before `Geometries`, fixture identifiers after geometry-related content, and `CustomIdType` before `CustomId`.

## Standalone MVR and project fixture metadata

Standalone export and project `scene.mvr` now share the Perastage Canonical MVR contract documented in [canonical_mvr_contract.md](canonical_mvr_contract.md). Both include root-level `ProjectFixtureMetadataMap`, scoped by `Data provider="Perastage" ver="1.0"` and its own `schemaVersion="1.0"`. Entries are sorted by canonical fixture UUID and preserve the editable `fixtureId`, `fixtureIdNumeric`, `fixtureIdText`, and `unitNumber` values independently from any interchange-only repair. They also preserve instance-owned `visualColorHex` values, including an explicit empty value through `hasVisualColorHex="false"`, without applying one fixture's override to other fixtures of the same type.

`ProjectFixtureMetadataMap` is canonical Perastage scene-fidelity metadata, not project-container-only state. Supported metadata is recovered during project restore and standalone open/replace imports. Merge imports recover the imported fixture metadata and carry it through the existing UUID remapping, without changing unrelated fixtures already in the target scene. A valid instance entry overrides weaker type-level color metadata. The reader keeps the first successfully parsed identifier entry for a UUID; duplicate valid color metadata is diagnosed and the first value takes precedence. Malformed UUIDs and unsupported map or provider versions are diagnosed, invalid color values are ignored with diagnostics, incomplete identifier sets are ignored, and entries whose UUID does not identify an imported fixture are diagnosed. Data from foreign providers never gains authority over Perastage-owned fixture metadata.

## Standard and compatibility truss geometry export

The parameterless exporter APIs, project `scene.mvr`, MVR-xchange, and canonical snapshots use `CanonicalMvrExportOptions()` and preserve the standard Symbol/Symdef truss representation when possible. File > Export MVR resolves the persisted standalone-export preference explicitly: **Standard MVR representation** uses that canonical policy, while **Direct Geometry3D for truss symbols** expands supported Symbol/Symdef geometry on the exporter's private scene copy. The latter is a standards-valid MVR 1.6 compatibility representation for applications with incomplete Symbol/Symdef support; it does not mutate or replace the canonical scene.

Invalid or missing `visualColorHex` values are ignored with the structured `invalid_project_fixture_visual_color` diagnostic. Standalone MVR identifier serialization treats `FixtureID` text independently from `FixtureIDNumeric`: duplicate numeric values are repaired to globally unique positive integers, while meaningful non-numeric text is preserved. Empty text and numeric fallback text follow the repaired numeric value. These standards-compliant exported substitutions do not overwrite project-owned identifiers when a `.pstg` project is restored, so intentional zero values and duplicate programming identifiers remain unchanged until the user edits them.

## Symbol/Symdef UUID handling

MVR `Symbol` nodes represent geometry instances defined by a referenced `Symdef`. MVR 1.6 requires every exported `Symbol` to carry both a canonical `uuid` attribute for the symbol instance and a non-empty `symdef` reference. Perastage preserves an imported Symbol UUID when it is valid and belongs to preserved Symbol/Symdef geometry; if the original Symbol UUID is missing, invalid, or collides with another exported Symbol, Perastage derives a deterministic replacement from the container type, container UUID, Symdef UUID, Symbol matrix, and stable index context so repeated exports of the same scene remain logically stable.


## MVR-xchange request/import behavior

Perastage can now request an advertised MVR revision from a compatible MVR-xchange TCP Mode station. The request uses the standard `MVR_REQUEST` message with the selected `FileUUID`, expects the peer to return an MVR file packet, writes the payload to a temporary `.mvr` file, and then runs the same MVR import policy used by **File > Import MVR...**.

The dialog lists advertised remote revisions from the discovered station list so the user can choose the exact MVR file to request. Publishing still uses `MVR_COMMIT` announcements, while requesting/importing keeps the MVR payload unchanged and relies on the existing importer for project integration. After a payload is received, Perastage shows the normal import choice so users can open it as a new project or merge it into the current project.

Perastage binds the mDNS responder to the interface selected for the advertised MVR-xchange address. When multiple adapters exist on the same computer, select the same network interface in Perastage and the peer application so service discovery and TCP file exchange use the same local link.
