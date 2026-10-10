# Opening MVR Files

## Open or import

Use **File → Import MVR...** and choose:

- **Open as new project** to replace the scene after the usual unsaved-project checks.
- **Merge into current project** to add content while keeping your existing work.

Opening an `.mvr` directly through startup or a supported operating-system file association opens it as a new project. Cancelling the import choice leaves the current project unchanged.

Review the imported scene in [2D/3D](views.md) and the Fixtures, Trusses, Hoists and Objects tables. Use the [Inspector](inspector.md) to examine a package without importing it.

## Merge conflicts

When UUIDs collide, choose whether to create new UUIDs for incoming objects (the default, keeping both scenes), replace matching project objects, or skip incoming colliding objects.

When a fixture type name refers to a different GDTF definition or mode, choose the current project definition, keep the imported definition under a renamed type, or cancel the merge. A cancelled or failed merge restores the current project.

Duplicate DMX patch addresses are non-blocking warnings in the Console. Review and correct the patch after merging.

## Resources and project storage

Imported GDTFs, symbols and models are retained with project resources so they can be resolved after saving and reopening. Check missing profiles and modes before export; see [GDTF Download](gdtf-download.md) and [Troubleshooting](troubleshooting.md).

Save ongoing work as `.pstg`, which preserves Perastage project context, layouts and project symbols. MVR is the exchange format. Perastage stores its own exchange metadata in standards-compatible, application-owned UserData; other applications may not retain that metadata.

## Export MVR

1. Choose **File → Export MVR...**.
2. Choose a destination; the suggested filename uses the project name when available.
3. Export and check the result in the receiving application.

[Preferences](preferences.md#mvr-import--export) offers a truss-geometry compatibility option. Project symbols saved in PSTG are not exported into MVR or embedded GDTFs.
