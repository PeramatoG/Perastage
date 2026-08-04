# GDTF resource identity and replacement reuse

## Identity boundaries

Perastage deliberately keeps five identities separate:

- **Source import identity** is the original `GDTFSpec`, internal FixtureTypeID, and
  imported type key. Two source archives remain distinct even when their labels are
  similar.
- **Selected replacement identity** is the explicit GDTF Share revision (RID) plus
  the resolved mode. It is authoritative for one import transaction and does not
  depend on an editable fixture type label.
- **Project-owned resource identity** is the physical GDTF stored for the scene.
  Every fixture assigned the same selected replacement uses the same path.
- **Symbol-generation identity** remains the portable project GDTF reference, mode,
  symbol format version, and strict semantic fingerprint used by the symbol cache.
- **Fixture type label** is presentation metadata and is never proof that resources
  are interchangeable.

## Root cause and correction

### Proven behavior

The importer grouped conflict rows by imported type label and previously downloaded
one file named after each row. Consequently, two legitimate WYSIWYG source aliases
that the user mapped to the same catalog revision produced different local paths.
Fixture assignment retained those paths. The exporter only reused identical source
paths, so different copies proposed the same canonical archive name and its safe
collision policy added numeric suffixes. Each packaged path then became a distinct
symbol-generation identity; an incomplete sibling correctly failed strict manifest
validation and caused real work on a later open.

The import queue now interns successful downloads by selected revision and compatible
mode. Later aliases reuse the first canonical file and all fixtures in each affected
alias are rebound to that shared scene-relative reference during the existing apply
phase. Similar names alone still do not merge. Export also reuses GDTFs whose complete
bytes prove equality, providing a conservative boundary for pre-existing exact
copies without weakening filename collision handling or manifest validation.

### Project-resource consolidation

After project resources and the strict saved manifest are restored, a core service
computes a separate base/source fingerprint. It includes every archive entry and the
parsed authoritative description, except the known four generated SVG files, the six
SVG offset attributes written for their model, a revision whose Perastage author and
`Applied fixture SVG symbol views (...)` text prove its purpose, and supported
Perastage symbol audit metadata. Unknown audit versions make the resource ineligible.
The strict symbol fingerprint remains unchanged and continues to validate exact saved
bytes.

Resources group only when this base fingerprint and exact mode match. A valid complete
manifest-backed symbol archive wins; an unsuffixed candidate wins the next tie, then a
lexical path tie-break is used. The complete plan is validated before any fixture is
rebound. Load bookkeeping marks the restored project saved first, applies the plan,
and marks it dirty only when references changed, before startup symbol scheduling.
Consequently a complete survivor avoids generation, while a group with no valid
survivor generates once. User edits, physical changes, unknown metadata, and arbitrary
Perastage revisions remain authoritative and prevent consolidation.

No non-standard GDTF or MVR metadata is introduced by this change. Permissive reads,
canonical writes, required symbol views, and exact packaged-byte manifest validation
remain unchanged.
