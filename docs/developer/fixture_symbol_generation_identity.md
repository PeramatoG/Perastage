# Fixture symbol generation identity

Checkpoint 04 replaces editable fixture names as symbol-work identifiers with a
GUI-independent contract owned by `core/symbols`.

## Audited legacy paths

The previous identity originated in `BuildFixtureSymbolCacheKey`, which preferred
`Fixture::typeName`, and flowed through `BuildFixtureAutoUpdateKey`, MainWindow's
initial queue and processed-key set, `ProjectFixtureSymbolIdentity`, project fixture
collection, packaged snapshot construction, cache-miss planning, validation requests,
manifest entries, manifest load/save/validation/update, round-trip tests, and the
`fixtureKey` diagnostics. The audit found no other symbol-work deduplication identity.
Presentation summaries also use type labels, but they do not control work equality.

## Version 1 generation identity

`FixtureSymbolGenerationIdentity` contains:

1. the normalized, project-owned GDTF archive reference;
2. the exact case-sensitive GDTF mode string;
3. the Perastage symbol format version; and
4. the existing semantic fingerprint of the selected GDTF archive.

An optional display label is retained for diagnostics and is excluded from equality.
The persisted key starts with `perastage-fixture-symbol-generation:v1|`, followed in
the field order above by each UTF-8 value serialized as its decimal byte length, a
colon, and the exact bytes. Length-prefixing is the escaping rule, so delimiters in a
field cannot make the representation ambiguous. No `std::hash` value is persisted.

Portable archive references use `/`, must be relative, and may not contain empty,
`.` or `..` components, backslashes, control characters, drive prefixes, or an
absolute root. Temporary extraction paths are used only to read bytes and never enter
the identity. Mode spelling is not case-folded.

When an absolute project path must first be converted, Windows drive paths are parsed
lexically on every host rather than through `std::filesystem`. Both supported
separator spellings are accepted, drive letters and path components are compared
case-insensitively, and the emitted relative reference preserves component spelling.
Different drives, paths outside the root, traversal above a drive root, ambiguous
relative roots, and UNC paths are rejected. POSIX absolute paths retain native lexical
relative-path behavior.

## Planning, persistence, and failure policy

Runtime work coalesces only after resolution and semantic fingerprinting produce the
complete key. Exact keys share one synchronous job; label changes do not create work,
while archive, mode, semantic, or format changes do. Resolution or fingerprint
failure produces a diagnostic and the fixture remains isolated by its queue UUID; a
label is never substituted as validation proof.

Manifest format 2 stores the canonical key and its audited component fields. Duplicate
keys deterministically replace the earlier record. Format 1 remains permissively
readable, but its name-keyed entries are not imported and cannot validate format 2
requests. Unknown future formats are likewise non-authoritative. A successful save
rebuilds and writes only format 2 from the exact GDTFs packaged in `scene.mvr`, using
the packaged reference, mode, bytes, and the same identity builder.

Checkpoint 05 consumes this identity for precise parsed-SVG invalidation. It does not
alter rendering, capture, scheduling, or thread ownership.
