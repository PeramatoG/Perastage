# Persistent application caches

Perastage stores regenerable geometry and GDTF catalog caches in its user data
directory. Core services manage cache paths, migration, size and safe cleanup.

`AppPaths` owns the canonical `<UserDataDir>/cache` root and its children:

- `viewer3d/gdtf`: mode-specific binary geometry.
- `viewer3d/meshes`: binary model geometry.
- `gdtf/catalog.json`: validated GDTF Share catalog snapshot.

Core's `CacheService` provides directory creation, statistics, cache deletion,
legacy library cleanup and Viewer3D key generation without GUI dependencies.
Preferences consumes this service; confirmation and translated presentation
belong to `CachePreferencesPage`. Clearing is immediate and Apply/OK do not
repeat it. Files under this root must be regenerable; settings, diagnostics,
credentials, dictionaries, projects and libraries must remain outside it.

Viewer keys use full SHA-256 of length-delimited normalized physical path
identity and GDTF mode, with separate model/GDTF namespaces. Binary headers
retain format version, source size and nanosecond modification-time validation.
Writes recreate directories after deletion. Existing runtime geometry stays
usable; subsequent disk misses regenerate caches through the normal loaders.
Catalog refresh reads disk each time and treats deletion as a miss.

The catalog migrates a usable legacy `gdtf_catalog_cache.json` from application
user data (including the historical wxWidgets and temporary fallback locations).
It validates before publication, preserves the old file on failure, and never
replaces an existing new catalog. A migration marker under
`<UserDataDir>/cache_migrations/gdtf_catalog` survives Clear Cache to prevent
leftover legacy copies from resurrecting discarded data.

Startup cleanup runs before library bootstrap on the installed library and
Perastage-owned user-data libraries, including the historical temporary fallback.
Only files beside existing supported assets in `fixtures`, `trusses`,
`scene_objects` and `hoists` qualify. Cleanup requires an exact asset-derived
`.cache` or GDTF decimal mode-hash filename, a matching MSCH/GGCH magic and a
known binary format version. Binary caches are deleted, never relocated.
Externally configured libraries are excluded. Cleanup is repeatable; unrelated
`.cache` files, orphan filenames and symlink targets are preserved.

Clearing recognizes Viewer3D SHA-256 `.cache` names and the catalog file and
its temporary/backup siblings; unrecognized files are preserved and reported,
even if placed inside the cache tree. Statistics and clearing report filesystem
failures, reject symlink directory
traversal and leave symlink entries untouched. Clearing recreates the required
cache directories and reports partial deletion through the GUI. The cache root
is exclusively application-owned: do not place user resources there.

Focused coverage lives in `cache_service_test`, `gdtf_catalog_cache_test` and
`preferences_dialog_test`; the cache boundary policy test guards GUI separation.
