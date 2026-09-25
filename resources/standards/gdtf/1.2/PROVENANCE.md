# Official GDTF 1.2 schema provenance

- **Format/version:** General Device Type Format (GDTF) 1.2.
- **Normative reference:** DIN SPEC 15800:2022-02.
- **Stable specification basis:** `mvrdevelopment/spec@098d3791f77f0895bd859adf01864b4826e2006f`.
- **Schema source:** `mvrdevelopment/tools/gdtf.xsd` at revision `e199c6ed635de23cb5ebf9654ee54a358775a065`.
- **Integrity:** `gdtf.xsd` is vendored byte-for-byte from that pinned source; SHA-256 `13a044d297f19d0437965657fb21d02f7b2b02541aabeba5f11e5000074ac2f2`.
- **Use:** CMake embeds the pinned bytes for deterministic offline validation; runtime validation does not access the source tree or network.
- **Redistribution:** No SPDX or license identifier is asserted for this upstream XSD while upstream clarification is pending. Its exact source and integrity are retained here so distribution treatment can be adjusted at the schema-provider boundary.
- **XSD 1.0 limits:** The schema header lists rules requiring higher-level parser checks, including conditional cardinality, DMX name uniqueness, attribute/feature references, and default ChannelFunction name collisions. Package resources and interoperability rules also remain outside XSD.
- **Interpretation:** Schema validity is structural evidence only and is not complete GDTF semantic or interoperability compliance.
