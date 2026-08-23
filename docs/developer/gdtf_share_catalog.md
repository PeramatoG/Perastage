# GDTF Share catalog ingestion

Perastage parses GDTF Share catalog payloads once through
`mvr/gdtf_catalog_parser`. The Search GDTF dialog filters the resulting shared
records, and automatic MVR replacement passes those same record types to the
catalog matcher. Records without a revision identifier remain displayable for
diagnostics but are explicitly non-downloadable.

The parser accepts the current catalog list and the `data`/`list` cached form.
The additional `fixtures`, `results`, `items`, `docs`, `rows`, and `catalog`
wrappers are compatibility-only shapes retained from Perastage's former search
dialog parser.

## Fixture identifiers

GDTF 1.2 defines `FixtureTypeID` as the unique identifier stored on the
`FixtureType` element. The GDTF Share catalog exposes a separate field named
`uuid`. Available official material reviewed for this change did not establish
that the Share field is byte-for-byte the GDTF `FixtureTypeID` for every
revision. Perastage therefore preserves both fields and includes them in
diagnostics, but does not equate them for automatic matching. Exact identifier
ranking can be enabled only after that cross-system contract is confirmed by an
authoritative API definition.

Catalog diagnostics use payload size and an FNV-1a fingerprint. The fingerprint
is an identity aid, not a security checksum, and no credentials or session data
are included.
