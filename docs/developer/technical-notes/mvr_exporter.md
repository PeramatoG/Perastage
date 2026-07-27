# MVR exporter warning behavior

Perastage MVR export now distinguishes between fatal validation errors and non-fatal resource warnings.

## Non-fatal warnings

When `GeneralSceneDescription.xml` references a `fileName`/`GDTFSpec` entry that is missing from the archive source set, or the same archive path appears more than once, export continues and records a warning:

- Missing file warning: `Referenced file '<name>' is missing from the archive and will be omitted.`
- Duplicate file warning: `Referenced file '<name>' appears multiple times; duplicates will be ignored.`

Warnings are available through `MvrExporter::GetExportWarnings()` after `ExportToFile(...)`/`ExportToBuffer(...)`.

## Fatal errors

Structural MVR compliance errors still fail export (for example missing `GeneralSceneDescription`, invalid version, or invalid required IDs).

## UI behavior

GUI code should show export warnings only after any busy overlay is destroyed, so dialogs remain visible to the user.
## MVR node validity notes

Support is a standard MVR scene node and is preserved as `<Support>` during import/export; if geometry is missing, Perastage writes an empty `<Geometries/>` plus required `ChainLength` so the logical Support remains schema-valid without inventing model geometry. Generic `<SceneObject>` nodes require a `<Geometries>` child, so Perastage writes a small 10 cm placeholder cube when no source geometry is available instead of writing invalid XML. Truss children are emitted in XSD `xs:sequence` order, with `Matrix` before `Geometries`, fixture identifiers after geometry-related content, and `CustomIdType` before `CustomId`.

## Standalone MVR and project fixture metadata

Standalone MVR export remains the canonical interchange contract. It emits no direct Fixture extension children and retains `FixtureTypeInfoMap` as portable type-level Perastage metadata. Project save explicitly enables an additional root-level `ProjectFixtureMetadataMap`, scoped by `Data provider="Perastage" ver="1.0"` and its own `schemaVersion="1.0"`. Entries are sorted by canonical fixture UUID and preserve instance-owned `visualColorHex` values, including an explicit empty value through `hasVisualColorHex="false"`, without applying one fixture's override to other fixtures of the same type.

Only `ProjectRestore` consumes this project-only map. External and merge imports ignore it. Within project restore, a valid instance entry overrides weaker type-level color metadata. The reader accepts the first valid duplicate deterministically, diagnoses later duplicates, rejects malformed UUIDs and unsupported versions, and diagnoses entries whose UUID does not identify an imported fixture. Foreign providers never affect project fixture state.

Invalid or missing `visualColorHex` values are ignored with the structured `invalid_project_fixture_visual_color` diagnostic. Fixture identifier serialization treats `FixtureID` text independently from `FixtureIDNumeric`: duplicate numeric values are repaired to globally unique positive integers, while meaningful non-numeric text is preserved. Empty text and numeric fallback text follow the repaired numeric value.

## Symbol/Symdef UUID handling

MVR `Symbol` nodes represent geometry instances defined by a referenced `Symdef`. MVR 1.6 requires every exported `Symbol` to carry both a canonical `uuid` attribute for the symbol instance and a non-empty `symdef` reference. Perastage preserves an imported Symbol UUID when it is valid and belongs to preserved Symbol/Symdef geometry; if the original Symbol UUID is missing, invalid, or collides with another exported Symbol, Perastage derives a deterministic replacement from the container type, container UUID, Symdef UUID, Symbol matrix, and stable index context so repeated exports of the same scene remain logically stable.


## MVR-xchange request/import behavior

Perastage can now request an advertised MVR revision from a compatible MVR-xchange TCP Mode station. The request uses the standard `MVR_REQUEST` message with the selected `FileUUID`, expects the peer to return an MVR file packet, writes the payload to a temporary `.mvr` file, and then runs the same MVR import policy used by **File > Import MVR...**.

The dialog lists advertised remote revisions from the discovered station list so the user can choose the exact MVR file to request. Publishing still uses `MVR_COMMIT` announcements, while requesting/importing keeps the MVR payload unchanged and relies on the existing importer for project integration. After a payload is received, Perastage shows the normal import choice so users can open it as a new project or merge it into the current project.

Perastage binds the mDNS responder to the interface selected for the advertised MVR-xchange address. When multiple adapters exist on the same computer, select the same network interface in Perastage and the peer application so service discovery and TCP file exchange use the same local link.
