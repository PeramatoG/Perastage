# GDTF mutation audit (Perastage)

Perastage centralizes GDTF write traceability in `core/gdtf_mutation_audit.{h,cpp}`.

## API

- `EnsureFixtureType(...)`
- `EnsureRevisionsNode(...)`
- `AppendRevision(...)`
- `StampPerastageMutationMetadata(...)`

These helpers are the single integration point for metadata that Perastage owns
when mutating `description.xml` inside `.gdtf` archives.

## Perastage mutation schema version

`kPerastageGdtfMutationSchemaVersion` is a Perastage-owned version marker for the
shape and semantics of Perastage mutation metadata (for example the
`<PerastageMutationAudit>` node and related attributes).

- It is **independent** from GDTF format versioning.
- It should be bumped only when Perastage changes its own mutation-audit contract.

## Relationship with Perastage app version

Each mutation stamp persists the running app version (`app::kVersion` and display
variant) alongside the schema version. In short:

- **Schema version** = contract version for audit metadata structure.
- **App version** = concrete Perastage build that performed the mutation.

This allows distinguishing "which metadata format" from "which app wrote it".
