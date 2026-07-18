# Portable dictionary bundle format

Perastage keeps JSON dictionary snapshots for backward compatibility and also supports a portable ZIP bundle format for fixtures and trusses dictionaries.

## Backward compatibility

- Existing `.json` snapshots are still valid for import/export.
- The **Import into active...** action accepts both JSON snapshots and ZIP bundles and merges them into the active dictionary without changing the active path.
- The **Export...** action offers exactly two choices: **JSON Snapshot** and **Portable ZIP Bundle**.
- **JSON Snapshot** writes a JSON snapshot with file references only.
- **Portable ZIP Bundle** writes a manifest-backed ZIP bundle that includes dictionary data and required assets.


## Read-only load and lookup rules

Dictionary loading and ordinary lookup operations must be side-effect free. Missing fixture and truss asset references remain in the active dictionary so the Dictionary Editor can display, repair, replace, or delete them explicitly. Lookups that validate asset paths return no match for unresolved references, but they must not delete entries, save the dictionary, create backups, normalize truss keys on disk, or migrate legacy truss assets during load. Truss legacy/model conversion belongs only in explicit import, add, replace, repair, or save workflows that own the asset-copy transaction.

## Active dictionary workflows

The Dictionary Editor exposes separate actions for active dictionary management:

- **Open...** validates an existing JSON dictionary and switches the active path only after the selected file matches the current page type.
- **New...** creates either an empty dictionary or a self-contained dictionary from application defaults, then activates it after validation succeeds.
- **Duplicate Current...** creates an independent working copy of the active dictionary, copies resolvable owned assets into the duplicate's storage location, preserves unresolved references with a summary, validates the duplicate before activation, and lets the user choose whether to activate it immediately.
- **Use Default** switches back to the managed user dictionary path for the current page type. It does not reset dictionary contents.
- Dirty prompts and discard/reload behavior are scoped to the current page type, so fixture actions do not prompt for truss edits and truss actions do not prompt for fixture edits.
- If an active custom dictionary is invalid or missing, normal writes are blocked until the user explicitly opens another dictionary, switches to the managed default, or recreates the active custom dictionary from application defaults.

These actions are grouped under the active dictionary section: **Open...** and **New...** stay visible, while **Duplicate Current...**, **Use Default**, and **Reset Contents...** live in **More...**. They are distinct from importing snapshots, exporting snapshots, exporting portable bundles, and resetting the active dictionary contents.

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

Portable ZIP bundle import has two separate core phases:

1. **Prepare** identifies ZIP input, extracts it to a unique temporary staging directory, rejects unsafe archive paths, validates `manifest.json`, validates required files, checks declared sizes and checksums, rejects inconsistent duplicate asset metadata, and writes a staged preview snapshot whose asset references still point at staging. Preparation is read-only with respect to active dictionary JSON and active asset storage. Cancelling after prepare must not leave copied assets, backups, rewritten active JSON, or other active-storage side effects.
2. **Apply** runs only after the user selects the import policy and confirms. It stages destination assets, reuses same-content assets, deterministically renames different-content filename conflicts, rewrites entries to final owned references, validates the staged result, backs up active asset storage, installs assets, applies the dictionary merge policy, and restores the asset backup if the import fails.

JSON snapshots remain separate from portable ZIP bundles. Snapshot import continues to use the existing JSON workflow and never changes the active dictionary path.

Exported portable ZIP bundles are validated through the same side-effect-free bundle validation path after creation. Export validation must not copy the exported bundle back into active storage.

## Collision policy when copying into library

Bundle preparation never overwrites active assets. During Apply, same-content
assets reuse the existing final file. If a staged bundle asset has the same
filename as an unrelated active asset, the imported asset is renamed
deterministically to `<basename>_<hash>.<ext>`.

Fixture and truss dictionary entries may include optional provenance/hash
metadata to aid diagnostics:

- `sha256`: content hash stored with the entry.
- `imported_at`: UTC ISO-8601 timestamp of when the entry was imported.

## `dictionary.json`

The bundle also includes `dictionary.json` using the existing dictionary JSON contract (`kind: perastage.dictionary`), which keeps compatibility with existing tooling and simplifies inspection/debugging.
