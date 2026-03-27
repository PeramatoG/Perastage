# Portable dictionary bundle format

Perastage keeps JSON dictionary snapshots for backward compatibility and also supports a portable ZIP bundle format for fixtures and trusses dictionaries.

## Backward compatibility

- Existing `.json` snapshots are still valid for import/export.
- The current **Import dictionary...** action accepts both JSON snapshots and ZIP bundles.
- The current **Export dictionary...** action still writes JSON snapshots.
- A new **Export portable bundle...** action writes a ZIP bundle that includes dictionary data and required assets.

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

## `dictionary.json`

The bundle also includes `dictionary.json` using the existing dictionary JSON contract (`kind: perastage.dictionary`), which keeps compatibility with existing tooling and simplifies inspection/debugging.
