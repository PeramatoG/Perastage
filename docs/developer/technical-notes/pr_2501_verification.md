# PR 2501 corrective verification

## Project save

Baseline `e3d0806d` rejects a synthetic MVR 1.6 Fixture containing
`CastShadow` and `CustomCommands`, after a Fixture name/transform edit and a
Layout text edit. `RecordStandardPreservationRisks` retains both source
subtrees, but `CheckStandardPreservation` rejects them before the archive can
be completed. The baseline diagnostic is `StandardDataPreservationBlocked /
Error / ExportFailed` at `/GeneralSceneDescription/Scene/Layers/Layer/ChildList/Fixture/CastShadow`
and the corresponding `CustomCommands` path. This does not identify the cause
of the user's private file failure.

The regression input is independently validated against the pinned official
MVR 1.6 XSD. Both fields now survive normal project save, actual project reload,
and a second save. The original source archive remains byte-identical. Embedded
and standalone canonical MVR have identical ordered entry names and payloads.
The test also inspects the actual output XML values, edited transform/name and
stored Layout text.

| Finding | Save/export behavior |
| --- | --- |
| Equivalent supported normalization | Normal canonical save/export; retain semantic values and provenance. |
| Valid retained standard field with modeled owner | Validate and project into standard XML; normal save/export, no consent loop. |
| Missing authored GDTF or retained source resource | Resource error; preserve target and dirty state; never replace an imported definition with a dummy. |
| Malformed retained XML, ambiguous reference or invalid identity | Specific error; preserve target and dirty state. |
| Invalid destination, close or atomic replacement failure | Archive IO error; preserve target; clean temporary file. |
| Other unsupported authored structure | Still unresolved; no silent loss or success claim. |

The latter row includes whole VideoScreen/Projector nodes, parametric ChildList
ownership and symbolic FocusPoint geometry. A general recovery-copy workflow
has **not** been implemented. Therefore this commit does not satisfy an
unconditional-save requirement for every valid imported standard structure.
The PR must remain unmergeable until these remaining source representations
have a tested preservation/recovery owner. Disabling the preservation gate is
not a resolution.

Save As now changes current path/display name only after success; recent-file
updates remain on that same success branch. Save preparation rolls back its
state on failure. Archive publication closes its output and atomically replaces
the destination, without a copy-overwrite fallback. All error diagnostics are
available from the dialog's details control after the busy overlay closes.
The UI boundary test checks ordering; the native UI test checks overlay lifetime
on both failure and exception.

Verified in the Linux Debug toolchain:

```
cmake --build build/wsl-x64-debug --target project_preserved_fields_roundtrip_test mvr_standard_preservation_test mvr_export_preparation_test project_session_io_test project_save_ui_test -j4
ctest --test-dir build/wsl-x64-debug -R '^(ProjectPreservedFieldsRoundtrip|ProjectSessionIO|MvrStandardPreservation|MvrExportPreparation|ProjectSaveUiBoundary)$' --output-on-failure
xvfb-run -a ctest --test-dir build/wsl-x64-debug -R '^ProjectSaveUi$' --output-on-failure
cmake --build build/wsl-x64-debug --target perastage_check_translations
```

Five Core/boundary tests and one native GUI test pass. Existing archive/resource
validation tests remain enabled. Physical disk exhaustion and the Windows
atomic replacement API have not been exercised here; Windows/macOS manual
validation remains pending.
