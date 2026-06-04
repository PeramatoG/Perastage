# Opening MVR Files

Perastage is built for MVR-centered workflows.

## Open an MVR file

Use one of these methods:

- Open an existing `.mvr` file directly.
- Use **File → Import MVR...** to choose how the selected MVR should enter the project.

Opening an `.mvr` file directly from the command line, operating system file association, or startup path uses the clean **Open as new project** behavior.

After import, review fixtures, trusses, hoists, objects, and layout information in the available views and tables.

## Import choices

When you import from the **File** menu, Perastage asks which workflow you want to use:

- **Open as new project** replaces the current scene with the selected MVR, after the usual dirty-project checks.
- **Merge into current project** keeps the current project and adds supported content from the selected MVR.
- Cancelling the choice leaves the current project unchanged.

## Merge behavior and prompts

Merging is designed to preserve the current project unless you explicitly choose a different conflict policy. Before applying a merge, Perastage imports the selected MVR into a temporary scene, analyzes conflicts, and only then applies the result to the live project. If a merge is cancelled or fails, the current project, layouts, selection, hidden layers, active layer, and dirty state are restored.

### UUID conflicts

If imported objects use UUIDs that already exist in the current project, Perastage prompts you to choose how to continue:

- **Create new UUIDs for imported objects** keeps both scenes by assigning stable replacement UUIDs to the incoming colliding objects. This is the default and safest choice for additive merges.
- **Replace existing project objects** lets incoming colliding objects take the place of matching current-project objects.
- **Skip incoming colliding objects** keeps the current objects and ignores incoming objects with the same UUIDs.

Perastage updates references inside the imported content, including positions, group children, layers, symbols, and hoist motor links, so the merged scene remains connected after UUID decisions are applied. Lookup data such as positions, layers, and symbol definitions is also handled without overwriting unrelated current-project state.

### Fixture type and GDTF conflicts

Perastage compares fixture type names against their resolved GDTF definitions before the merge is applied. If the imported MVR uses the same fixture type name as the current project but points to a different GDTF file, GDTF mode, or file content, Perastage prompts you to decide how every incoming fixture of that type should be resolved:

- **Use current project definition for imported fixtures** maps the imported fixtures to the current project's fixture type, GDTF file, and mode.
- **Keep imported definition by renaming it** preserves the imported GDTF decision under a generated fixture type name, such as `Example Type (Imported)`, so both definitions can coexist.
- **Cancel merge** stops the merge before any live project changes are applied.

### Non-blocking duplicate patch warnings

Duplicate DMX patch addresses do not block a merge. If incoming fixtures reuse patch addresses that already exist in the current project, Perastage completes the merge and reports a warning summary in the console. The warning tells you how many duplicate DMX address cases were found so you can review and resolve patch conflicts after the merged scene reloads.

### Imported resource handling

Referenced imported resources such as GDTF files, truss archives, symbols, and 3D models are copied into the current project resource area and rewritten when needed. This keeps existing and imported resources resolvable after saving and reopening the project.

## Perastage project files

In addition to MVR exchange, Perastage supports project-based work so you can:

- Save in-progress work.
- Reopen and continue edits later.
- Keep iterative scene revisions while preparing a final export.

## Export MVR

When your changes are ready for exchange:

1. Open **File → Export MVR...**.
2. Choose destination and file name.
3. Export the updated scene.

## Practical tip

If imported fixtures appear incomplete, download the required GDTF profiles first, then reopen or refresh your scene review workflow.
