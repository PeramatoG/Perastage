# MVR identity recovery

Perastage treats UUIDs as scene-instance identity, independently of display names, row order, GDTF filenames, and filesystem paths. Canonical editable and exported identity uses lowercase RFC 4122 text with hyphens.

`mvridentity::RecoverSceneIdentities` is the GUI-independent transaction boundary used before strict MVR serialization. It preserves canonical UUIDs, accepts uppercase, surrounding whitespace, braces, and unhyphenated UUID text, and deterministically replaces missing, malformed, duplicate, or key-versus-field-conflicting identities. It rebuilds scene maps and rewrites GroupObject parent and child references in the same transaction. The explicit import/domain migration boundary materializes default and legacy name-only layers once; the final export recovery remains defensive.

Each material change produces a structured diagnostic with object kind, display name, source context, original identity, replacement identity, and reason. A genuinely new object should still receive `GenerateUuid()` when it is created; deterministic recovery is reserved for imported or persisted identity damage.

Identity scopes follow the MVR 1.6 contract rather than assuming that every UUID-bearing element shares one global namespace. Normal scene objects are checked together. Layers, Positions, Symbols, and Symdefs retain their separate owned resolution paths, and references must resolve inside the applicable scope.


## Specification provenance

The authoritative specification basis is `mvrdevelopment/spec` at
`098d3791f77f0895bd859adf01864b4826e2006f`. Validation uses the exact official
`mvrdevelopment/tools/mvr.xsd` at `e199c6ed635de23cb5ebf9654ee54a358775a065`,
embedded with the repository's current XSD provenance and offline hash guard.
The XSD permits empty/duplicate Layer names and optional `Layer/ChildList`.
Perastage's exactly-one-ChildList output is an intentional deterministic
canonical-output policy, not an additional MVR identity mechanism.

## Persisted reference ownership

The shared scene-object alias scope rewrites Fixture, Truss, Support, SceneObject, and GroupObject parent links; GroupObject children; `Support::motorFixtureUuid`; and Layer child UUID compatibility lists. Exact and canonical source spellings are transaction aliases. An alias that targets multiple recovered objects is ambiguous, is never guessed, and produces a structured diagnostic. Unresolved or ambiguous supported references are cleared after diagnosis so the repaired model cannot retain a misleading target and a second recovery is idempotent.

Fixture Focus and Fixture/Truss/Support Position references remain in their existing separately resolved scopes. Truss and geometry Symbol/Symdef fields remain owned by the exporter/importer Symbol and Symdef resolvers. The current model does not persist general MVR multipatch, classing, mapping, or connection references, so identity recovery does not invent those unsupported fields or targets.

## Mutation and dirty-state policy

Import and legacy project load normalize Layer identities and ownership in the
editable scene. Production-created Layers receive canonical UUIDs immediately.
Export preparation calls recovery on a private copy. Neither standalone export
nor a read-only canonical snapshot mutates, migrates, or marks the live scene
dirty because recovery occurred. Project save serializes canonical MVR without
publishing defensive repairs back into the editable scene. Identity damage
introduced later can therefore still be diagnosed on subsequent exports; normal
Layer import/default creation no longer creates such damage in the first place.

Layer aliases rewrite `layerUuid` ownership for all five scene object types.
`Layer::name` never determines persistent membership. Default creation uses
`DeriveDeterministicUuid("mvr:layer:default")`; legacy `layer_default` loads map
to that same identity. Unnamed and equal-named foreign Layers remain independent.
See [Layer identity](layer_identity.md) for migration and presentation adapters.
