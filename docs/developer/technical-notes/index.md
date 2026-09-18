# Technical Notes

Technical notes are focused maintainer contracts below the project-wide owners
listed in the [Developer Documentation index](../index.md). An active note owns
only its named implementation area; it does not replace architecture,
repository, build, or subsystem policy. Point-in-time evidence is separated at
the end of this page.

## MVR contracts

- [Canonical MVR contract](canonical_mvr_contract.md) — canonical MVR format
  and behavior rules for Perastage.
- [MVR exporter diagnostics](mvr_exporter.md)
- [MVR-xchange TCP Mode publisher](mvr_xchange.md)
- [Canonical hoist metadata](mvr_hoistinfo_schema.md)
- [Truss geometry authority](mvr_truss_geometry_authority.md)
- [Truss metadata and resources](mvr_truss_metadata_resources.md)

The focused [MVR identity recovery](../mvr_identity_recovery.md),
[truss attachment candidates](../truss_attachment_candidates.md), and
[fixture attachment paths](../truss_fixture_attachment_paths.md) contracts live
one level above because they span MVR and scene-model ownership.

## GDTF contracts

- [GDTF Unicode ZIP filename compatibility](gdtf_unicode_zip_filename_compatibility.md)
- [GDTF editor architecture and checkpoint contracts](gdtf-editor/gdtf_editor_architecture_audit.md)
- [GDTF editor context boundaries](gdtf-editor/gdtf_editor_context_boundaries.md)
- [GDTF editor UI layout](gdtf-editor/gdtf_editor_ui_layout.md)
- [GDTF read/write compatibility](gdtf-editor/gdtf_read_write_compatibility.md)
- [GDTF mode and channel browser](gdtf-editor/gdtf_mode_channel_browser.md)
- [GDTF wheel and attribute inspector](gdtf-editor/gdtf_wheel_attribute_inspector.md)

These notes defer mutation and derivative identity rules to the canonical
[GDTF Mutation Policy](../gdtf_mutation_policy.md) and
[GDTF resource identity](../gdtf_resource_identity.md) documents.

## Viewer and rendering contracts

- [Viewer2D rendering and capture](viewer2d_rendering.md)
- [Viewer2D state ownership](viewer2d_state_ownership.md)
- [Print Viewer2D options](print-viewer2d-options.md)
- [OpenGL lifecycle](opengl_lifecycle.md)
- [Scene-model symbol capture](scene_model_symbol_capture.md)
- [Basic geometry primitives](basic_geometry_primitives.md)
- [Viewer3D performance stress scenario](viewer3d_performance_stress_scenario.md)

## Runtime and storage contracts

- [Runtime storage and temporary workspaces](runtime_storage_and_temp_workspaces.md)

## Historical / validation evidence

These documents retain useful checkpoint evidence, not current policy:

- [GDTF mutation audit](gdtf_mutation_audit.md) — superseded as an authority by
  the [GDTF Mutation Policy](../gdtf_mutation_policy.md).
- [GDTF Editor Checkpoint 08D regression matrix](gdtf-editor/gdtf_editor_checkpoint08d_regression_matrix.md)
  — a checkpoint test record; current editor contracts are linked above.
