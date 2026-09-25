# Perastage MVR 1.6 schema provenance

- **Format/version:** My Virtual Rig (MVR) 1.6.
- **Normative reference:** DIN SPEC 15801:2023-12.
- **Implementation source:** `mvrdevelopment/spec`, revision `098d3791f77f0895bd859adf01864b4826e2006f`.
- **Ownership:** This XSD is maintained by Perastage as a machine-readable implementation derived from the referenced specification. It is not an official GDTF/MVR Group schema.
- **Development comparison:** Representative fixtures under `tests/fixtures/standards/mvr/1.6/` were compared with `mvrdevelopment/tools/mvr.xsd` at revision `e199c6ed635de23cb5ebf9654ee54a358775a065`; the upstream schema is not vendored or downloaded by tests.
- **Structural coverage:** The schema enforces the MVR 1.6 root version and metadata, optional `UserData` followed by required `Scene`, required `Layers`, UUID shape and uniqueness, and the scene node, hierarchy, Symdef, Position, geometry, and GDTF reference structures used by inspection.
- **XSD 1.0 limits:** Graph/reference integrity, packaged resource existence, ZIP naming, root-entry location, conditional node rules, and foreign provider payload semantics remain semantic or package checks.
- **Intentional differences:** Unlike the backwards-compatible upstream comparison XSD, the version-specific Perastage 1.6 schema requires `provider` and `providerVersion`, as required by MVR 1.6. Foreign provider payloads remain opaque by design. XSD success is not complete MVR interoperability compliance.
