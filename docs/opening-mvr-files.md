# Opening MVR Files

Perastage is built for MVR-centered workflows.

## Open an MVR file

Use one of these methods:

- Open an existing `.mvr` file directly.
- Use **File → Import MVR...**. After choosing a file, select **Open as new project** to replace the current scene or **Merge into current project** to add the selected MVR content to the current scene.

When merging, Perastage checks imported object UUIDs against the current project and gives colliding imported objects stable replacement UUIDs by default. If collisions are found, you can instead choose to replace existing project objects or skip incoming colliding objects. References inside the imported content, such as positions, group children, layers, and hoist motor links, are updated so the merged scene remains connected while existing project objects are preserved.

Opening an `.mvr` file directly from the command line, operating system file association, or startup path uses the clean **Open as new project** behavior.

After import, review fixtures, trusses, hoists, objects, and layout information in the available views and tables.

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
