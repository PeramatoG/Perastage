# Perastage GDTF 1.2 schema provenance

- **Format/version:** General Device Type Format (GDTF) 1.2.
- **Normative reference:** DIN SPEC 15800:2022-02.
- **Implementation source:** `mvrdevelopment/spec`, revision `098d3791f77f0895bd859adf01864b4826e2006f`.
- **Ownership:** This XSD is maintained by Perastage as a machine-readable implementation derived from the referenced specification. It is not an official GDTF Group schema.
- **Development comparison:** Representative fixtures under `tests/fixtures/standards/gdtf/1.2/` were compared with `mvrdevelopment/tools/gdtf.xsd` at revision `e199c6ed635de23cb5ebf9654ee54a358775a065`; the upstream schema is not vendored or downloaded by tests.
- **Structural coverage:** The schema models required fixture metadata, attribute definitions and collections, ordered fixture families, common wheel/physical/model/geometry/DMX structures, scalar and enumeration types, uniqueness constraints, and top-level geometry references.
- **XSD 1.0 limits:** Some conditional cardinality, default-name uniqueness, feature and DMX cross-references, resource existence, and archive rules require semantic or package validation. Less commonly consumed leaf structures remain candidates for further specification-derived expansion.
- **Intentional differences:** No behavioral difference exists for the committed comparison fixtures. Perastage uses a narrower explicitly modeled leaf vocabulary than the comparison schema rather than silently accepting defined standard structures through wildcards. XSD success is not complete GDTF compliance.
