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

Support is a standard MVR scene node and is preserved as `<Support>` during import/export; if geometry is missing, Perastage writes an empty `<Geometries/>` plus required `ChainLength` so the logical Support remains schema-valid without inventing model geometry. Generic `<SceneObject>` nodes require a `<Geometries>` child, so Perastage writes a small placeholder cube when no source geometry is available instead of writing invalid XML. Truss children are emitted in XSD `xs:sequence` order, with `Matrix` before `Geometries` and fixture identifiers after geometry-related content.
