# Layer identity and compatibility boundaries

MVR Layer UUIDs own persistent identity. `Layer::name` is authored presentation
metadata; empty and equal names are valid in the repository's official MVR 1.6
schema. Imported layers are never coalesced or renamed to satisfy a GUI label
policy. Canonical UUID spelling may normalize representation without replacing
the effective UUID. The specification/XSD basis remains the pinned provenance
in [Architecture](architecture.md#xml-and-standards-validation-layers).

`Fixture`, `Truss`, `Support`, `SceneObject`, and `GroupObject` own `layerUuid`.
Export, grouping, deletion, Query membership, visibility, and current-layer
selection use UUIDs. Rename changes metadata and refreshes derived labels; it
never changes object ownership. An authored foreign layer called `No Layer` is
not special. Only the canonical application default UUID identifies the default
layer, and only that identity is protected from user rename/deletion.

Core's GUI-independent `layer_identity` API owns UUID lookup, presentation
labels, compatibility resolution, default creation, and migration. Empty names
are presented as `[<UUID>]`; equal names get a full UUID suffix. These labels
never overwrite `Layer::name` or enter standard MVR fields. User-created names
still follow the separate LayerService UI policy: nonempty, bounded, and unique.
Imported names are not subject to that policy.

## Default and legacy migration

The application default is a normal Layer named `No Layer`, with UUID
`DeriveDeterministicUuid("mvr:layer:default")`. Fresh project setup and production
creation allocate canonical identities immediately. `layer_default` is accepted
only as legacy input and maps deterministically to this default UUID. No
production creation path emits that token or a `layer_<timestamp>` identity.

The shared MVR reader retains every Layer, even empty layers with empty names.
It preserves valid UUIDs, repairs malformed/missing/conflicting source identities
once in the parsed editable scene, and assigns children by the parsed Layer UUID.
Source metadata used by Inspector remains separately recorded as authored.
The legacy `layer_default` spelling uses the same canonical default policy.

`MigrateLayerMembership` is an explicit import/mutation compatibility boundary.
For missing ownership, it prefers explicit child ownership, then unambiguous
legacy label/name resolution. Absent legacy layers are inferred deterministically
from their names. Ambiguous legacy assignments without child ownership are kept
in a distinct inferred layer; migration does not guess between existing UUIDs.
Repeated migration is idempotent. A nonempty authoritative ownership reference
is never replaced by a name merely because the label disagrees.

## Project and workspace state

`.pstg` persists scene identity through canonical `scene.mvr`, so the standard
Layer/ChildList graph carries UUID ownership without a private MVR extension.
Project restore uses the same reader and migration. `view_hidden_layers` now
stores UUIDs; older name entries are resolved on load. A historical hidden name
shared by several layers hides those same layers after migration. Current-layer
selection is saved as `view_current_layer_uuid`; old projects lacking that key
select the canonical application default. History snapshots include UUID state
and object ownership, preserving Undo/Redo across rename and delete.

## Remaining compatibility text

The old object `layer` string remains a **derived label**, because existing table
widgets, natural sorting, and parts of GUI cache invalidation consume display
text. Table adapters prepare label edits without mutating the scene, then commit
UUID ownership after the Undo snapshot; creation adapters call `AssignLayer`. All persistent
ownership decisions use `layerUuid`. Replacement/conversion paths copy UUID
ownership independently. `GetLayerNames` remains a presentation API returning
unique labels; UUID enumeration is separate and does not collapse names.
Low-level `LayerVisibilityState` still accepts string tokens for old callers;
ConfigManager resolves project/workspace inputs through the domain API before
publication, and render/picking paths consume UUIDs. Legacy MVR appearance maps
may resolve colors by name only when UUID-keyed appearance is unavailable; that
fallback never chooses membership or changes identity.

Follow-up: replace the remaining table display strings with explicit UUID/value
choices and compute labels at presentation time. This would remove the derived
object label cache without changing the domain or standard serialization.

## Defensive export boundary

`RecoverSceneIdentities` remains enabled. Export preparation and canonical
snapshots operate on a private copy; read-only snapshots do not migrate or dirty
the live scene. Import and legacy load store migration results in editable state,
so normal export does not repeatedly repair application-generated identities.
Malformed, missing, duplicate, conflicting, or unresolved identities introduced
later still produce the established diagnostics. Representation normalization
and source identity preservation remain separate concerns.
