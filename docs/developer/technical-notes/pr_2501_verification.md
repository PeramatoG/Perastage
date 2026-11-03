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
stored Layout text. A separate source-field merge regression verifies that
uppercase UUID aliases follow collision remaps for both field owners and
Connection references, rather than losing a field or retargeting a relation.

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

## Root cause locations

The following line numbers refer to the examined baseline `e3d0806d`:

| Cause | Location |
| --- | --- |
| Valid source-only fields recorded, then rejected before the owning writer could serialize them | `mvr/mvr_standard_preservation.cpp:28`, `:180`; `core/configmanager.cpp:593` |
| Only the first export error reached the save failure details | `core/configmanager.cpp:607` |
| Save As changed session path before success | `gui/mainwindow_io.cpp:389` |
| Rename failure fell back to overwriting by copy | `core/configservices.cpp:998` |
| Separate Legend metric/geometry profiles despite one algorithm | `core/layouts/LegendComposition.h:31`, `LegendComposition.cpp:71`, `:183` |
| Platform wx measurement versus PDF legacy font metrics/compensation | `gui/layoutlegendrenderer.cpp:430`, `:436`; `viewer2d/pdf/layout_pdf_legend_renderer.cpp:37`, `:63` |
| Rounded, scaled frames fed back into PDF composition | `gui/mainwindow_print.cpp:392` |

## Legend parity

The production adapters now use the bundled PerastageSans metrics and the
existing `typography::Compose` engine. The same logical frame produces the
same name lines, row frames, header baselines, text-cell clips, symbol slots and
column widths. Preview draws positioned glyph contours; PDF embeds the same
font and retains selectable vector text. There are no Legend target profiles,
platform font measurements or typography compensation factors. Event Table
and free-text typography remain unchanged. Command-symbol stroke units are
resolved before drawing and shared by both targets. Output paper scaling is
one uniform fit after composition, preserving aspect ratio and the selected
output page size/orientation.

At a 180 x 100 point frame, with three visible rows, all three enabled symbol
views, Count and Ch columns, the actual adapters assert:

| Fixture | Reported original preview / PDF mismatch | New preview and PDF nameLines |
| --- | --- | --- |
| Exterior Linear-1200 Graze | Exterior / Linear-1200 Graze versus Exterior Linear-1200 / Graze | Exterior / Linear-1200 Graze |
| MAC Ultra Performance | MAC Ultra / Performance versus one line | MAC Ultra / Performance |
| Martin Atomic 3000 LED | Martin Atomic / 3000 LED versus one line | Martin Atomic / 3000 LED |

The original screenshot's page/frame dimensions are unavailable, so this is a
controlled reproduction with the exact names, not a reconstruction of its
private layout. At 300 points wide all three names fit on one line on both
targets. The two-line remainder stays in the plan; its cell clips visual
overspill rather than painting into Ch. Font changes from accidental legacy
platform/PDF choices are intentional; wrapping is now deterministic.

Production-adapter tests cover 0/1/3/5/20/100 rows, 120/180/300/600 point widths,
preview zooms 50/80/100/200%, and PDF scales 0.5/1/2. They compare complete plan
positions and inspect the actual PDF drawing stream's text/baseline commands.
The PDF document test checks embedded font resources, vector paths for each
view, all toggle combinations, counts, mixed page ordering and independence of
a second Legend, Event Table, free text, view and image. `pdftotext` also
extracts all three exact names from the 300-point production-exporter PDF.

UTF-8 diacritics and contiguous long names remain valid at every split.
Unsupported UTF-8 remains in source data; the bounded WinAnsi repertoire omits
unsupported glyphs consistently on both targets and reports line/byte/hex
findings, capped at 16 details with an uncapped total count. PDF completion
exposes those details. Full Unicode shaping/rendering is not claimed.

`LayoutLegendRasterParity` runs both production adapters, renders actual PDFs
with Poppler and normalizes each page to four pixels per document point. It
checks per-text-cell dark-ink bounding boxes within one raster pixel (0.25 pt),
and mean absolute RGB error below 3/255, excluding the two-pixel page-edge crop.
Independent plan/stream assertions require identical logical coordinates;
raster tolerances cover antialiasing and font hinting, not layout differences.
Results are identical across all three PDF output scales at each width:

| Width (pt) | Mean absolute RGB error (/255) | Maximum ink bounds delta (px) |
| --- | --- | --- |
| 120 | 0.8178 | 0 |
| 180 | 2.1953 | 1 |
| 300 | 1.0379 | 1 |
| 600 | 0.7963 | 1 |

The generated preview PNGs and PDF rasters were also inspected. Reproduce and
retain those review artifacts with:

```
python3 tests/check_legend_raster_parity.py --preview build/wsl-x64-debug/tests/layout_legend_preview_test --pdf build/wsl-x64-debug/tests/layout_legend_pdf_test --xvfb xvfb-run --pdftoppm pdftoppm --output /tmp/perastage-legend-evidence
```

This test uses only Python's standard library plus optional test tools
Xvfb/Poppler; it adds no application dependency or font binary. It is registered
on Linux when those tools are available. Native Windows/macOS visual validation
and interactive Windows PDF-viewer zoom review remain pending.

## Combined verification and merge gate

The Debug application builds and starts under Xvfb; the live CLI returns a
successful `scene.summary`. All **51 selected CTest checks pass**, including
real MVR/PSTG round trips, source matrix precision, fixture-label override
reconciliation, Layout image/resource and history regressions, MVR-xchange
protocol, native save UI lifetime, production Legend adapters and raster parity.
This is a focused suite, not a full repository/hosted-CI pass.

After building the corresponding test targets, the exact selected suite is:

```
ctest --test-dir build/wsl-x64-debug --output-on-failure -j2 -R 'Layout|Legend|Pdf|HistoryManager|FixtureSymbolSvgCache|FixtureSymbolResolution$|ProjectFixtureSymbol(Runtime|ReadContext|s)$|ProjectSession|FixtureSymbolProjection|ProjectPreserved|MvrStandardPreservation|MvrExportPreparation$|MvrMatrixPrecision|MvrXchangeProtocol|FixtureLabelOverridesReconcile|ProjectSupportUserDataRoundtrip|ProjectSaveUi'
```

The required module-tree, GUI ConfigManager, source-size and repository-hygiene
guards, `git diff --check` and `perastage_check_translations` pass. No official
schema or persisted Layout/PSLAYOUT schema changed, and no new application
assets/dependencies or source-size exceptions were added.

Review the project-save commit independently before the Legend commit. B1's
Linux automated parity gate passes. P0's general unsupported-source save/export
and recovery gate remains **open**, as detailed above. Keep PR 2501 open as a
draft; **do not merge** until those preservation cases have an honest tested
save/reopen/export route and required platform checks pass.
