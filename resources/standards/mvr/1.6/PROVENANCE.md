# Perastage MVR 1.6 schema provenance

- **Format/version:** My Virtual Rig (MVR) 1.6.
- **Normative reference:** DIN SPEC 15801:2023-12.
- **Implementation source:** `mvrdevelopment/spec`, revision `098d3791f77f0895bd859adf01864b4826e2006f`.
- **Ownership:** This XSD is maintained by Perastage as a machine-readable implementation of the referenced specification. It is not an official GDTF/MVR Group schema.
- **Development comparison:** `mvrdevelopment/tools/mvr.xsd`, revision `e199c6ed635de23cb5ebf9654ee54a358775a065`, was used only as a behavioral comparison reference and is not vendored here.
- **XSD 1.0 limits:** UUID/reference integrity, graph relationships, packaged resource existence, ZIP naming, root-entry location, and conditional rules remain semantic or package checks. Foreign provider data remains opaque.
- **Known difference:** This focused schema validates the stable root contract and document ordering while leaving scene vocabulary to the existing reader and semantic layer; it does not reproduce the upstream schema's complete element vocabulary.
