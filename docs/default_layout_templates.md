# Default layout templates for new projects

Perastage can preload layout templates when creating a **new** project.

## Folder

Place template files in:

- `library/default_layouts/`

The directory is resolved through `ProjectUtils::GetDefaultLibraryPath("default_layouts")`,
so packaged defaults can be copied to the user library fallback when needed.

## File format

- One or more `.json` files.
- Each file must follow the layout template schema consumed by
  `layouts::FromTemplateDocument(...)`.
- Files are processed in alphabetical order.
- Invalid files are ignored; Perastage continues with the remaining files.

If no valid templates are found, Perastage keeps the existing behavior and starts
with the built-in empty layout.

## Image paths in templates

`imageViews[].path` supports:

1. **Absolute paths** (kept as-is).
2. **Relative paths to the template file directory**.
3. **Relative paths to app resources**:
   - `resources/<file>` (for example `resources/Perastage_logo.png`).

At load time, relative paths are resolved to absolute paths when the file exists.

## Scope

Template loading is only applied when the user uses **New Project**.
It does not alter loading of existing project files.
