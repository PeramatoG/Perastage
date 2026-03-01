# Tests notes

## MVR long-path extraction regression

The MVR importer now remaps archive entries to `/_long/<hash>.<ext>` when direct extraction would exceed safe Windows path limits (MAX_PATH-compatible mode) or when file creation fails.

When adding MVR parsing changes, please validate these cases:

1. A normal MVR with short file names still preserves original archive-relative references.
2. A long-path MVR still imports and resolves `GDTFSpec` and `Geometry3D/@fileName` references through the remap table.
3. Logs include the remap warning with original entry name and original full path length.
