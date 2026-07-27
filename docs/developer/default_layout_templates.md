# Default layout templates

Perastage can preload layout templates from `library/default_layouts/`.

## Folder

Place template files in:

- `library/default_layouts/`

The directory is resolved through `ProjectUtils::GetDefaultLibraryPath("default_layouts")`,
so packaged defaults can be copied to the user library fallback when needed.

## File format

Preferred default layouts use the portable `.pslayout` package format. A
`.pslayout` file is a ZIP archive with this versioned structure:

- `manifest.json`
- `layout.json`
- `resources/layout_images/<content-addressed image files>`

`manifest.json` declares `format: "perastage-layout-package"`,
`packageVersion: 1`, the layout schema version, and the layout entry point.
`layout.json` contains exactly one layout template document using the existing
layout schema.

Legacy `.json` layout template files remain readable for compatibility, but new
exports no longer create standalone JSON templates. Default template files are
processed in deterministic alphabetical order. Invalid files are ignored;
Perastage continues with the remaining files.

If no valid templates are found, Perastage keeps the existing behavior and starts
with the built-in empty layout.

## Image portability

Portable `.pslayout` files must be self-contained. Export rewrites image element
metadata so both `path` and `projectResource` reference the packaged
`resources/layout_images/` entry, and `originalPath` is omitted to avoid leaking
local source paths. Image payloads are content-addressed and deduplicated inside
the package. Layout package and project archive resource names are canonical
UTF-8 ZIP paths: they are relative, use `/` separators on every platform, and
reject backslashes, drive-qualified paths, absolute paths, and traversal. ZIP
directory entries are metadata and never count as packaged image payloads.

When a package is imported, Perastage materializes image files into owned runtime
storage, updates the runtime `imagePath`, preserves `projectResourcePath`, and
registers the bytes so later `.pstg` project saves can package the same images.

## Legacy JSON image paths

Legacy `imageViews[].path` values still support:

1. **Absolute paths** when the file exists on the current machine.
2. **Relative paths to the template file directory**.
3. **Relative paths to app resources**:
   - `resources/<file>` (for example `resources/Perastage_logo.png`).

Legacy JSON templates are not self-contained. Prefer recreating bundled defaults
as `.pslayout` packages when image portability is required.

## Security restrictions

Portable package readers reject unsafe archive paths, including absolute paths,
empty entry names, backslash-separated names, `.` or `..` traversal components,
duplicate critical entries, missing manifests, missing entry points, unsupported
package versions, invalid JSON, and image references outside
`resources/layout_images/`. Conservative entry count and payload size limits are
applied to avoid pathological packages.

## Scope

Template loading is applied when:

- The user uses **New Project**.
- The active config has no stored `layouts_collection` yet (for example first
  launch with an empty config).
- The stored `layouts_collection` value exists but is empty/invalid.

If a project already has stored layouts, template files are ignored.
