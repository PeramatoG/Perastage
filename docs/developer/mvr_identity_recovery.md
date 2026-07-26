# MVR identity recovery

Perastage treats UUIDs as scene-instance identity, independently of display names, row order, GDTF filenames, and filesystem paths. Canonical editable and exported identity uses lowercase RFC 4122 text with hyphens.

`mvridentity::RecoverSceneIdentities` is the GUI-independent transaction boundary used before strict MVR serialization. It preserves canonical UUIDs, accepts uppercase, surrounding whitespace, braces, and unhyphenated UUID text, and deterministically replaces missing, malformed, duplicate, or key-versus-field-conflicting identities. It rebuilds scene maps and rewrites GroupObject parent and child references in the same transaction. Default and legacy name-only layers are materialized once with deterministic UUIDs; subsequent save, reload, and export preserve that stored value.

Each material change produces a structured diagnostic with object kind, display name, source context, original identity, replacement identity, and reason. A genuinely new object should still receive `GenerateUuid()` when it is created; deterministic recovery is reserved for imported or persisted identity damage.

Identity scopes follow the MVR 1.6 contract rather than assuming that every UUID-bearing element shares one global namespace. Normal scene objects are checked together. Layers, Positions, Symbols, and Symdefs retain their separate owned resolution paths, and references must resolve inside the applicable scope.


## Specification provenance

The authoritative schema reference is `mvrdevelopment/tools/mvr.xsd` at commit `16f9ff3624d3e715798a28b2c460579c55820853` (Git blob `b250b81a1a98f5dbeaf7eb55c54e21409d83f829`, 28,286 bytes, SHA-256 `c6dadedf91d9bd93148f2fdb3843cfca9c8f8ec0b86f035cbb4110def74af5ef`). The XSD declares `Layer/ChildList` with `minOccurs="0"`. Perastage's exactly-one-ChildList output is an intentional deterministic canonical-output policy and remains valid, but it is not an MVR 1.6 schema requirement.

## Persisted reference ownership

The shared scene-object alias scope rewrites Fixture, Truss, Support, SceneObject, and GroupObject parent links; GroupObject children; `Support::motorFixtureUuid`; and Layer child UUID compatibility lists. Exact and canonical source spellings are transaction aliases. An alias that targets multiple recovered objects is ambiguous, is never guessed, and produces a structured diagnostic. Unresolved or ambiguous supported references are cleared after diagnosis so the repaired model cannot retain a misleading target and a second recovery is idempotent.

Fixture Focus and Fixture/Truss/Support Position references remain in their existing separately resolved scopes. Truss and geometry Symbol/Symdef fields remain owned by the exporter/importer Symbol and Symdef resolvers. The current model does not persist general MVR multipatch, classing, mapping, or connection references, so identity recovery does not invent those unsupported fields or targets.

## Mutation and dirty-state policy

Import and project load already normalize several identity scopes, while export remains the defensive boundary for damaged editable or legacy project state. When defensive recovery changes the scene, export marks the project dirty before serialization. A successful project save persists the repaired scene and then marks it saved. Standalone MVR export leaves the repaired editable scene dirty, making the side effect visible. If publication later fails, the deterministic repair remains in memory and remains dirty rather than being silently rolled back or lost; retrying recovery is idempotent and uses the same service.
