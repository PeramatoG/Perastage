# MVR identity recovery

Perastage treats UUIDs as scene-instance identity, independently of display names, row order, GDTF filenames, and filesystem paths. Canonical editable and exported identity uses lowercase RFC 4122 text with hyphens.

`mvridentity::RecoverSceneIdentities` is the GUI-independent transaction boundary used before strict MVR serialization. It preserves canonical UUIDs, accepts uppercase, surrounding whitespace, braces, and unhyphenated UUID text, and deterministically replaces missing, malformed, duplicate, or key-versus-field-conflicting identities. It rebuilds scene maps and rewrites GroupObject parent and child references in the same transaction. Default and legacy name-only layers are materialized once with deterministic UUIDs; subsequent save, reload, and export preserve that stored value.

Each material change produces a structured diagnostic with object kind, display name, source context, original identity, replacement identity, and reason. A genuinely new object should still receive `GenerateUuid()` when it is created; deterministic recovery is reserved for imported or persisted identity damage.

Identity scopes follow the MVR 1.6 contract rather than assuming that every UUID-bearing element shares one global namespace. Normal scene objects are checked together. Layers, Positions, Symbols, and Symdefs retain their separate owned resolution paths, and references must resolve inside the applicable scope.
