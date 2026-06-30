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

Support is a standard MVR scene node and is preserved as `<Support>` during import/export; if geometry is missing, Perastage writes an empty `<Geometries/>` plus required `ChainLength` so the logical Support remains schema-valid without inventing model geometry. Generic `<SceneObject>` nodes require a `<Geometries>` child, so Perastage writes a small 10 cm placeholder cube when no source geometry is available instead of writing invalid XML. Truss children are emitted in XSD `xs:sequence` order, with `Matrix` before `Geometries` and fixture identifiers after geometry-related content.

## Symbol/Symdef UUID handling

MVR `Symbol` nodes represent geometry instances defined by a referenced `Symdef`. MVR 1.6 requires every exported `Symbol` to carry both a canonical `uuid` attribute for the symbol instance and a non-empty `symdef` reference. Perastage preserves an imported Symbol UUID when it is valid and belongs to preserved Symbol/Symdef geometry; if the original Symbol UUID is missing, invalid, or collides with another exported Symbol, Perastage derives a deterministic replacement from the container type, container UUID, Symdef UUID, Symbol matrix, and stable index context so repeated exports of the same scene remain logically stable.


## MVR-xchange request/import behavior

Perastage can now request an advertised MVR revision from a compatible MVR-xchange TCP Mode station. The request uses the standard `MVR_REQUEST` message with the selected `FileUUID`, expects the peer to return an MVR file packet, writes the payload to a temporary `.mvr` file, and then runs the same MVR import policy used by **File > Import MVR...**.

The dialog currently requests the latest advertised remote revision from the discovered station list. Publishing still uses `MVR_COMMIT` announcements, while requesting/importing keeps the MVR payload unchanged and relies on the existing importer for project integration.
