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
| VideoScreen/Projector, media, parametric children, multipatch and standard Symbol topology | Retain validated structure/resources around edited typed nodes; normal canonical save/export. |
| Nested Symdef graphs and edited instance geometry | Typed nested instances, cycle/reference checks, isolated definition for an edited instance; retain original/unused definitions. |
| Model with available external textures/buffers | Preserve original model bytes and relative resource tree in a stable package namespace. |

The follow-up replaces the missing structural preservation owners with
`mvr_source_structure_capture/projection/edits/merge.cpp`. The former importer
flattened geometry for display and retained only neutral descriptors for media
nodes. The ordinary writer consequently lacked their topology and payloads;
preflight correctly refused loss. The new source evidence is immutable,
scene-owned and reconstructed from canonical XML on reopen. Ordinary typed
writers still own editable fields. This removes the listed representation
blockers instead of suppressing diagnostics or maintaining an alternate PSTG
scene. A recovery-copy consent path is unnecessary for these covered cases.
For editable owners, structural validation covers the restored geometry and
hierarchy; existing identifier/DMX/network validators remain authoritative,
including published-standard cases that disagree with the pinned XSD (signed
identifiers and IPv6). Source-only media payloads use their pinned standard
type. Native editor range limits retain the existing bounded policy.

`ProjectStructured*Roundtrip` independently validates synthetic standard input,
then imports it, edits existing Fixture name/transform, a nested Fixture,
Truss placement and real Layout text. Six variants exercise basic structure,
edited instance geometry, external textures, their combination and collision
merge with/without textures. They inspect actual output node types, ownership,
Symbol UUIDs/references, source-only Classing and a numeric identifier beyond the
editor's int32 range, media and texture bytes. Both the standard Scene and its
editable fields survive actual PSTG reopen and three saves. They compare all
ordered embedded/standalone canonical ZIP entry payloads and unchanged source
archive bytes. Explicit Direct Geometry3D compatibility remains a distinct
output. Mixed direct geometry and multiple Symbols share one owner; editing
one Symbol's geometry retains the other instance and original definitions.

Missing media/texture/GDTF (including an unrelated same-name file elsewhere),
invalid source identities, cyclic references, malformed retained fields and
invalid/write-failing targets retain separate failures, original target bytes,
dirty state and no temporary-file leaks. These are bounded preservation and
transaction checks, not certification of every provider extension or physical
disk-exhaustion behavior.

Save As now changes current path/display name only after success; recent-file
updates remain on that same success branch. Save preparation rolls back its
state on failure. Archive publication closes its output and atomically replaces
the destination, without a copy-overwrite fallback. All error diagnostics are
available from the dialog's details control after the busy overlay closes.
The UI boundary test checks ordering; the native UI test checks overlay lifetime
on both failure and exception.

Focused Linux Debug commands:

```
cmake --build build/wsl-x64-debug --target project_preserved_fields_roundtrip_test mvr_symdef_preservation_test mvr_merge_analyzer_applier_test -j4
ctest --test-dir build/wsl-x64-debug -R '^(Project(PreservedFields|Structured.*)Roundtrip|MvrSymdefPreservation|MvrMergeAnalyzerApplier|MvrSourceStructureBoundary)$' --output-on-failure -j2
cmake --build build/wsl-x64-debug --target perastage_check_translations
```

The focused structural/resource suite passes **20/20** tests, including the
independent Python Symdef/Focus/Class readers. The existing precision corpus
checks 19 Trusses under nested groups, unedited numeric provenance, one-ULP
translation, group movement, rotation/scale and repeated actual save/reload.
Source projection leaves the ordinary writer's prepared local matrix untouched
when its native owner and placement already match; it does not recompute and
narrow unrelated authored values during a parent edit. The first affected
resource remains available in the save-failure API alongside all detailed
findings.

The native save UI lifetime check runs under Xvfb. Physical disk exhaustion and
the Windows atomic replacement API have not been exercised here; Windows/macOS
manual validation remains pending.

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
successful `scene.summary`. All **79 selected CTest checks pass**, including
real MVR/PSTG round trips, source matrix precision, fixture-label override
reconciliation, Layout image/resource and history regressions, MVR-xchange
protocol, native save UI lifetime, production Legend adapters and raster parity.
This is a focused suite, not a full repository/hosted-CI pass.

After building the corresponding test targets, the exact selected suite is:

```
verification_suite='Layout|Legend|Pdf|HistoryManager|FixtureSymbolSvgCache|FixtureSymbolResolution$|ProjectFixtureSymbol(Runtime|ReadContext|s)$|ProjectSession|FixtureSymbolProjection|ProjectPreserved|ProjectStructured|MvrStandardPreservation|MvrExportPreparation$|MvrMatrixPrecision|MvrXchangeProtocol|FixtureLabelOverridesReconcile|ProjectSupportUserDataRoundtrip|ProjectSaveUi|Mvr(Symdef(Integrity|Independent|Preservation|Ownership)|Focus(Integrity|Independent)|Class(Integrity|Independent|Preservation)|Identifier(Integrity|Independent)|MergeAnalyzerApplier|SourceStructureBoundary|TrussRoundtripStructure|ExportResource(Collection|Boundary)$|ExportArchive(Writer|Boundary)$|FixtureAddresses|LayerMatrix)'
xvfb-run -a ctest --test-dir build/wsl-x64-debug --output-on-failure -j2 -R "$verification_suite"
```

The required module-tree, GUI ConfigManager, source-size and repository-hygiene
guards, `git diff --check` and `perastage_check_translations` pass. No official
schema or persisted Layout/PSLAYOUT schema changed, and no new application
assets/dependencies or source-size exceptions were added.

Review the project-save corrections independently from Legend composition.
The covered P0 structures have a normal canonical save/reopen/export route;
B1 retains its Linux automated parity gate. Keep PR 2501 open as a draft while
platform validation/review is pending; do not merge automatically. Neither
these tests nor the source-preservation service certify arbitrary future MVR
versions, provider extensions or unavailable external resources.
