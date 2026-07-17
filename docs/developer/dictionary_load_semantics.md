# Dictionary load semantics

Perastage keeps fixture and truss dictionaries deterministic: loading a valid existing dictionary is read-only. Lookups, autocomplete, rider import, fixture replacement, and opening the Dictionary Editor must not add application-default entries or save the active dictionary merely because defaults are missing.

## Managed default dictionaries

The managed default dictionaries live in the writable user library at `fixtures/gdtf_dictionary.json` and `trusses/truss_dictionary.json`. On first run, when one of these files is missing, Perastage creates it by copying the matching application base dictionary. This is initialization, not load-time merging. After the managed default file exists, user deletions remain deleted until the user explicitly imports or resets defaults.

## Custom active dictionaries

A custom active dictionary is any selected dictionary path outside the managed default path. Custom dictionaries own their referenced assets through their adjacent dictionary asset storage when assets are copied into them.

- **Open** validates and activates an existing dictionary.
- **New Empty** creates a valid dictionary with no entries; reopening it keeps it empty.
- **New From Defaults** seeds entries from application defaults once at creation time.
- **Duplicate Current** copies the active dictionary and its resolvable owned assets.
- **Use Default** switches back to the managed default dictionary path; it does not import defaults into a custom dictionary.

## Import, export, and reset

Import is an explicit operation using the selected import policy. Export writes a snapshot or portable bundle and does not change the active dictionary. Reset Active Dictionary to Application Defaults is a destructive explicit operation: the current active dictionary is backed up, then replaced with application defaults after user confirmation.

## Recovery behavior

Core dictionary load APIs expose structured status so the GUI can present precise recovery information:

- loaded active dictionary;
- active dictionary missing;
- active dictionary invalid;
- temporary fallback used;
- managed default recreated.

Invalid custom dictionaries are preserved and are not silently overwritten. If loading needs a fallback, Perastage may temporarily use the application base dictionary without changing the active custom file or its configuration. The managed default dictionary may be recreated from the application base only for first-run creation or a narrow recovery path, and recovery from an existing invalid managed default creates a `.bak` backup first. If the application base dictionary is invalid too, loading fails and reports both active and base errors.

The Dictionary Editor guards dirty state per page. Fixture actions only prompt for unsaved fixture-table edits, and truss actions only prompt for unsaved truss-table edits. Choosing **Discard** reloads only the guarded page from its active dictionary before the requested action continues, leaving unrelated edits on the other page untouched.

Before a normal write, the editor checks the guarded page load status. If the active custom dictionary is invalid, missing, or currently shown through a temporary application-default fallback, writes are blocked until the user explicitly chooses **Open Another Dictionary...**, **Use Default**, or **Recreate Active Custom Dictionary From Application Defaults...**. Cancel leaves the configured path and existing file bytes unchanged. Recreating an active custom dictionary uses the normal dictionary creation path, including asset ownership handling and a `.bak` backup when a file already exists.
