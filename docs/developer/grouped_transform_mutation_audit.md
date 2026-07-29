# Grouped transform mutation audit

Perastage treats each scene node's editable world matrix as the visible
placement and stores a local matrix for MVR hierarchy persistence.

## Corrected user-editing paths

- Fixture, Truss, Support, and SceneObject table transforms use the shared
  exact-world mutation boundary.
- Primitive SceneObject placement and object-level scale changes use the same
  exact-world boundary; geometry-entry scale remains geometry-local.
- Fixture-to-Support conversion preserves UUID, world/local transforms, parent
  metadata, and changes the existing GroupObject child-reference type.
- Table deletion uses the typed core removal service, removes parent child
  references, and recursively prunes groups that become empty.

Deleting a GroupObject explicitly deletes its complete subtree. Deleting leaf
nodes preserves non-empty ancestors and prunes empty ancestors. This is a
Perastage editing policy; it does not change MVR import hierarchy semantics.

## Reviewed-safe paths

- MVR import builds world transforms from parent-world and local matrices.
- Grouping, ungrouping, and reparenting preserve world placement through the
  scene-grouping service.
- selected-Truss replacement preserves world/local/parent metadata.
- SceneObject-to-Truss conversion preserves transform metadata and repoints the
  GroupObject child-reference type.
- Undo and Redo restore complete scene snapshots rather than replaying direct
  transform writes.
- Renderer, loader, and view caches contain derived transforms and do not own
  editable hierarchy state.

## Follow-up review areas

Duplication, paste, and alignment/distribution paths should continue to use the
exact or interactive core boundaries according to their user-visible intent.
