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

Filesystem viewer keys use full SHA-256 of length-delimited normalized physical
path identity and GDTF mode, with separate model/GDTF namespaces. Extracted MVR
3DS/GDTF keys instead use a deterministic package payload manifest digest and
normalized archive resource identity, independent of temporary extraction paths.
The manifest includes companion resources such as textures and is independent
of ZIP entry order/compression/timestamps. Payloads are hashed during extraction.
Workspace/scene leases retain these identities across history snapshots; no
registration outlives its resources. A subsequent physical modification rejects
the registered identity. OBJ/GLB remain filesystem keyed pending dependency-aware
packaged caching.

Binary headers retain format version and source size. Filesystem assets validate
nanosecond modification time; registered packaged assets use the immutable
package identity and a zero timestamp marker, allowing reuse after re-extraction.
No source or canonical scene metadata changes. This shares persistent cached
processing across reopenings; CPU/GPU geometry sharing across different logical
resources is a separate optimization.

`ViewerCacheBatch` validates/prepares directories once per resource batch and
memoizes source identity calculation. Per-file and owned-ancestor symlink checks
remain live, including after a directory is replaced during a batch.
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
