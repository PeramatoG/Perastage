# Official MVR 1.6 schema provenance

- **Format/version:** My Virtual Rig (MVR) 1.6.
- **Normative reference:** DIN SPEC 15801:2023-12.
- **Stable specification basis:** `mvrdevelopment/spec@098d3791f77f0895bd859adf01864b4826e2006f`.
- **Schema source:** `mvrdevelopment/tools/mvr.xsd` at revision `e199c6ed635de23cb5ebf9654ee54a358775a065`.
- **Integrity:** `mvr.xsd` is vendored byte-for-byte from that pinned source; SHA-256 `85bc42015b706b2ab2bcc2ce69649e7245272b3606217263c90126cda21cb86c`.
- **Use:** CMake embeds the pinned bytes for deterministic offline validation; runtime validation does not access the source tree or network.
- **Redistribution:** No SPDX or license identifier is asserted for this upstream XSD while upstream clarification is pending. Its exact source and integrity are retained here so distribution treatment can be adjusted at the schema-provider boundary.
- **XSD 1.0 limits:** The schema is intentionally backwards-compatible. Its header requires parser-level MVR 1.6 checks for `provider`, `providerVersion`, Fixture `ChildList`, and the MVR 1.5-only `FixtureTypeId` element. Package resources and graph integrity remain outside XSD.
- **Interpretation:** Schema validity is structural evidence only and is not complete MVR semantic or interoperability compliance.
