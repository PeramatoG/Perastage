# Portable dictionary bundle format

Perastage keeps JSON dictionary snapshots for backward compatibility and also supports a portable ZIP bundle format for fixtures and trusses dictionaries.

## Backward compatibility

- Existing `.json` snapshots are still valid for import/export.
- The **Import into active...** action accepts both JSON snapshots and ZIP bundles and merges them into the active dictionary without changing the active path.
- The **Export snapshot...** action still writes JSON snapshots of the active dictionary.
- The **Export portable bundle...** action writes a ZIP bundle that includes dictionary data and required assets.
- JSON snapshot export can optionally copy referenced files to a sibling
  `<snapshot>_assets/assets/` folder and write `assets/...` relative paths.


## Read-only load and lookup rules

Dictionary loading and ordinary lookup operations must be side-effect free. Missing fixture and truss asset references remain in the active dictionary so the Dictionary Editor can display, repair, replace, or delete them explicitly. Lookups that validate asset paths return no match for unresolved references, but they must not delete entries, save the dictionary, create backups, normalize truss keys on disk, or migrate legacy truss assets during load. Truss legacy/model conversion belongs only in explicit import, add, replace, repair, or save workflows that own the asset-copy transaction.

## Active dictionary workflows

The Dictionary Editor exposes separate actions for active dictionary management:

- **Open...** validates an existing JSON dictionary and switches the active path only after the selected file matches the current page type.
- **New...** creates either an empty dictionary or a self-contained dictionary from application defaults, then activates it after validation succeeds.
- **Duplicate Current...** creates an independent working copy of the active dictionary, copies resolvable owned assets into the duplicate's storage location, preserves unresolved references with a summary, validates the duplicate before activation, and lets the user choose whether to activate it immediately.
- **Use Default** switches back to the managed user dictionary path for the current page type. It does not reset dictionary contents.

These actions are distinct from importing snapshots, exporting snapshots, exporting portable bundles, and resetting the active dictionary contents.

## Preflight validation and warnings

Both import and export run path checks before applying changes:

- Export preflight reports:
  - total entries,
  - entries with file found,
  - entries with missing file.
- Import preflight reports unresolved references and shows up to a small set of
  missing examples before asking whether to continue.

The import summary also tracks `missing_files_count` and example entries.

## ZIP layout

A portable bundle is a regular `.zip` file with the following entries:

- `manifest.json`
- `dictionary.json`
- `assets/*` (copied assets used by the exported dictionary entries)

No custom file extension is required.

## `manifest.json` contract

```json
{
  "kind": "perastage.dictionary_bundle",
  "format_version": 1,
  "dictionary_type": "fixtures",
  "dictionary_file": "dictionary.json",
  "entries": { ... },
  "assets": [
    {
      "path": "assets/MyFixture.gdtf",
      "checksum": "fnv1a64:0123456789abcdef",
      "size": 123456
    }
  ]
}
```

### Required fields

- `kind`: must be `perastage.dictionary_bundle`.
- `format_version`: bundle schema version (`1` currently).
- `dictionary_type`: `fixtures` or `trusses`.
- `entries`: dictionary entries for the selected dictionary type.
- `assets`: list of packed assets with path + checksum metadata.

## Import behavior for ZIP bundles

When a bundle is imported:

1. The ZIP is extracted to a temporary staging directory.
2. `manifest.json` is validated (`kind`, `dictionary_type`, required fields).
3. Every declared `assets[]` item is validated with its checksum.
4. Assets are copied to the target library directory:
   - fixtures -> `library/fixtures`
   - trusses -> `library/trusses`
5. Dictionary entries are rewritten so `file` references point to the imported library assets.
6. The rewritten dictionary is imported using the same import policy flow used for JSON snapshots.

## Collision policy when copying into library

When a source file would overwrite an existing filename in the target library
(`library/fixtures` or `library/trusses`) and contents differ, the user is
prompted to choose one policy:

- **Rename** to a deterministic `<basename>_<hash>.<ext>` target.
- **Overwrite** existing file.
- **Cancel** import for that asset.

Fixture and truss dictionary entries may include optional provenance/hash
metadata to aid diagnostics:

- `sha256`: content hash stored with the entry.
- `imported_at`: UTC ISO-8601 timestamp of when the entry was imported.

## `dictionary.json`

The bundle also includes `dictionary.json` using the existing dictionary JSON contract (`kind: perastage.dictionary`), which keeps compatibility with existing tooling and simplifies inspection/debugging.
