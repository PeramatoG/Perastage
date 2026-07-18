# Perastage runtime storage policy

Perastage separates persistent user data from runtime artifacts so imports, exports, generated resources, and caches have an explicit owner.

## Persistent user data

Project files, user dictionaries, fixture libraries, downloaded fixture assets, preferences, and user-selected source files remain in the existing Perastage user-data and project locations. Runtime cleanup must never delete these paths.

## Runtime root layout

New runtime artifacts are created below the Perastage-owned system temporary root:

```text
<SystemTemp>/Perastage/
  sessions/<session-id>/
    cache/v1/
  operations/
```

The runtime storage subsystem in `core/runtime_storage.{h,cpp}` is the authoritative API for this layout. Tests may inject a separate root so they do not use the developer's real temporary folders.

## Operation temporaries

Operation workspaces are represented by move-only RAII workspaces. They are unique directories below `operations/` and are deleted recursively on success, failure, cancellation, and exception unwind. Cleanup validates that the target remains inside the Perastage runtime root and logs failures instead of throwing from destructors.

## Scene and session resources

Imported MVR extraction roots, merge resource roots, and converted scene-object resources may outlive the operation that created them. These directories are transferred from an operation workspace into shared scene resource leases. `MvrScene` stores these runtime-only leases; copied scenes and Undo/Redo snapshots share the same lease, and the directory is released when the final scene owner disappears. The lease list is not serialized to MVR, PSTG, JSON, dictionaries, or UserData.

## Session caches

GDTF loader extractions, truss-generated GDTF cache entries, and primitive preview models use a session-scoped cache under `sessions/<session-id>/cache/v1/`. Cache keys include source identity where applicable so unchanged files are reused within a session. Removing a cache entry releases its owned files. Session caches are deleted on clean shutdown through normal lease/workspace destruction and recovered on the next startup when marked stale.

## Persistent bounded caches

No new persistent runtime cache is introduced by this policy. Cross-run caches must use a versioned cache root, last-access metadata, documented size limits, and explicit eviction before being added.

## Crash recovery and legacy cleanup

Each new session directory contains a Perastage marker file. Stale cleanup removes only marked session directories below the Perastage runtime root and never scans or deletes outside that root. Legacy direct children of the system temporary directory with broad prefixes such as `ps_` or `GDTF_` are not deleted automatically because those names are too generic; they may be reported for manual review. Unambiguous future legacy cleanup must require exact Perastage-owned patterns, age checks, containment checks, and link-safety checks.

## Archive extraction security

ZIP-derived workflows must use the shared safe archive extraction policy where compatible. Extraction rejects empty names when inappropriate, absolute paths, drive/root escape forms, `..` components, unsafe normalized destinations, and entries that exceed documented count or size limits. Incomplete workspaces remain operation-owned and are removed automatically on failure.
