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

## Additional audited paths

- Duplication constructs independent top-level nodes and intentionally clears
  inherited hierarchy ownership before insertion.
- Clipboard paste restores complete serialized scene records; it does not apply
  partial world-matrix edits to existing grouped nodes.
- Alignment and distribution use the interactive scene-grouping boundary, so
  grouped Trusses promote while other node types remain exact.
- Fixture and Truss replacement preserve the instance world/local matrices and
  parent metadata while changing type-level resource fields.
- Fixture-to-Support and SceneObject-to-Truss conversions now preserve or
  repoint hierarchy ownership through their owning conversion services.
- Grouping, ungrouping, Magnet commit/detach, and explicit reparenting use core
  hierarchy helpers that preserve world placement and rebuild local matrices.
- Primitive object placement uses the exact transform boundary; geometry-entry
  transforms are intentionally local geometry data rather than scene-node
  placement.
- Direct map erasure remains only in construction/import/reset code or inside
  the hierarchy-aware removal service. Table deletion no longer erases scene
  nodes directly.
- Undo and Redo replace the complete scene snapshot, including both transform
  matrices and GroupObject ownership, so no incremental hierarchy repair is
  required during restoration.

Permissive MVR reading, renderer/model-loader transforms, view caches, and
derived geometry matrices were reviewed but intentionally left unchanged
because they do not mutate editable scene-node hierarchy state.
