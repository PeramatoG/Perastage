# GDTF mutation audit (Perastage)

Perastage centralizes standard GDTF revision traceability in `core/gdtf_mutation_audit.{h,cpp}`.

## API

- `EnsureFixtureType(...)`
- `EnsureRevisionsNode(...)`
- `AppendRevision(...)`
- `StampPerastageMutationMetadata(...)`

These helpers are the single integration point for standard GDTF revisions that
Perastage writes when intentionally mutating `description.xml` inside `.gdtf`
archives. `StampPerastageMutationMetadata(...)` is retained as a compatibility
no-op for older call sites and must not write non-standard GDTF XML nodes.

## Legacy Perastage mutation schema version

`kPerastageGdtfMutationSchemaVersion` is a Perastage-owned version marker for the
legacy shape and semantics of Perastage mutation metadata (for example the
`<PerastageMutationAudit>` node and related attributes).

- It is **independent** from GDTF format versioning.
- Perastage does not write this node in new exports.
- It exists so compatibility code can identify legacy files written by older builds.

## Relationship with Perastage app version

Each standard GDTF revision uses `ModifiedBy="Perastage " + perastage::build_info::appVersion()`.
In short:

- **GDTF DataVersion** = GDTF compatibility version, for example `1.2`.
- **Revision ModifiedBy app version** = concrete Perastage build that performed
  the intentional mutation.

This keeps Perastage application versioning separate from GDTF format
compatibility versioning.
