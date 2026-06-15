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

Support is a standard MVR scene node and is preserved as `<Support>` during import/export when valid geometry is available. Generic `<SceneObject>` nodes require a `<Geometries>` child, so Perastage omits geometry-less SceneObjects instead of writing invalid XML. Truss children are emitted in XSD `xs:sequence` order, with `Matrix` before `Geometries` and fixture identifiers after geometry-related content.
