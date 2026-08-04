# Runtime fixture SVG cache

Checkpoint 05 gives parsed fixture SVG symbols an explicit GUI-independent owner in
`core/symbols/fixture_symbol_svg_cache.*`. The layout legend is the affected persistent
consumer. Layout view preview and print use that shared legend path. The dedicated
layout preview renderer, PDF exporter, fixture editor preview, and Viewer3D builders
load or build transient data and cannot retain a legend miss across refreshes.
Viewer2D's symbol cache stores scene symbol definitions rather than parsed GDTF SVG
data and is unrelated.

## Key and result contract

Each positive entry is equal only when the canonical physical archive identity,
`SymbolViewKind`, semantic fingerprint, optional canonical
`FixtureSymbolGenerationIdentity::key`, and SVG parser format version are equal.
Editable fixture and type names never participate. Runtime paths use
`PathUtils::BuildFilesystemIdentityKey`; they are not persisted and extraction paths
never become project identity.

Lookups return `shared_ptr<const PerastageSvgSymbolData>`. Published geometry is
immutable, and a handle remains valid after insertion, rehash, invalidation, or clear.
Rendering colors, transforms, scaling, and fills remain outside the cache. A mutex
protects map state and counters, but archive I/O and SVG parsing occur without that
mutex. Concurrent duplicate loads converge on the first published immutable entry.

Failures are not cached. Missing archives or views, malformed symbols, and loader
failures are returned as ordinary misses with the loader diagnostic, so creation of a
file or view can be observed on the next lookup even without a restart. Statistics
distinguish positive hits, loads, failures, path and identity invalidations, clears,
and live entries. Project lifecycle clearing bounds accumulation.

## Invalidation and ordering

The coordination API invalidates parsed SVG and semantic fingerprint state for a
physical path. Atomic GDTF replacement invokes it immediately after the replacement
commits and before post-write validation. Therefore a validation failure after
replacement cannot expose old parsed geometry; a failure before replacement leaves
the valid entry intact. Validation then recomputes and publishes the semantic
fingerprint before any refresh can reload the symbol.

A separately copied library derivative is invalidated independently before its
validated project fingerprint is published. A scene/library pair resolving to the
same physical path follows the scene mutation once. Optional library failure does not
invalidate the valid project archive and remains non-fatal.

Successful project load clears runtime parsed SVG and fingerprint state only after
the load commits, so a failed load retains the active project's cache. New-project,
reset, close, and switch paths pass through the reset boundary and clear both caches.
Fingerprint changes also miss naturally because the fingerprint is part of the key.
Path and generation-identity operations remain available for committed fixture
replacement or reassignment boundaries without exposing the container.

## Cache/load audit

1. `GetCachedLegendSvgSymbol` and `CachedSvgSymbolEntry` were affected: their static
   map retained positive and negative data and returned a raw pointer. They were
   removed in favor of the managed service.
2. `BuildSharedLayoutLegendItems` and `SharedLayoutLegendItem` are transient metadata
   builders. Preview and print consume the shared legend renderer and managed result.
3. The layout view renderer, PDF exporter, fixture editor preview, and Viewer3D SVG
   builder perform direct transient loads. They reload from validated archives and do
   not require another persistent cache.
4. Viewer2D and Viewer3D geometry/resource caches are keyed or rebuilt by their scene
   update lifecycle and do not store the legend's parsed SVG result.
5. `ApplySymbolsToFixtureGdtfWithResult`, `RewriteGdtfWithProof`, library derivative
   synchronization, semantic fingerprint publish/invalidate, project load, and reset
   are mutation or lifecycle boundaries and now coordinate the affected runtime cache.
6. The manifest and project snapshot remain versioned validity proof, not presentation
   caches; their format-2 identity and zero-work reload behavior are unchanged.

Checkpoint 06 and later still own readiness sequencing, scheduling, cancellation,
capture ownership, renderer batching, visibility priority, and concurrency. None of
those responsibilities are implemented here.
