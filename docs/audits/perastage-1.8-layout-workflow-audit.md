# Perastage 1.8.0 — Layout workflow, architecture and labels

## 1. Executive summary

Audit date: **2026-10-19**. Documentation only; no features implemented.
Baseline: current GitHub `main`, [20cd918a](https://github.com/PeramatoG/Perastage/commit/20cd918a0d4539e0831b22ab534e76daf0445ce1), version 1.7.18.
Git fetch was unavailable in this environment. GitHub's commit comparison verified
that the local 8b8bbdb1 checkout differs only in VERSION and
`mvr/mvr_model_dependencies.cpp`; all audited paths below match that main revision.

**C** = confirmed source behavior; **V** = runtime verification required;
**D** = proposed design; **R** = behavior documented by the reference vendor.
No application session or exported PDF was exercised. Source findings are not
claims of reproduced GUI defects. Recommendations below are **D** unless marked.
Reuse the [earlier architecture audit](perastage-1.8-documentation-cad-architecture.md)
for MVR preservation, CAD interoperability and detailed typography evidence.

The five most significant findings for a usable 1.8.0 plotting workflow are:

1. **C:** Page operations and element operations have inconsistent history entry points;
   layout snapshot transfer in Undo/Redo does not capture the departing document.
2. **C:** Pages support A3/A4 and orientation, but viewport framing remains pixel-camera
   based, without an explicit physical drawing-scale contract or precision inspector.
3. **C:** Fixture labels already use UUID overrides and live Name/ID/DMX values, but
   have no reusable field styles or viewport-local per-fixture adjustments.
4. **C:** Preview and PDF share scene commands but independently compose page text,
   legends and tables; font metrics, encoding, symbol fitting and strokes can differ.
5. **C:** Typed elements and resources are reusable, but mutation, capture and queries
   remain tied to GUI/global state, obstructing reliable headless document operations.

**D:** Deliver a coherent sheet editor around `LayoutCollection`, one presentation
resolver and shared derived page composition. Preserve `CommandBuffer`, the PDF
backend and resource services. Correct history before expanding editing; establish
physical units before adding paper-sized labels or dimensions. MVR preservation
remains the independent P0 obligation established in the earlier audit.

## 2. Current capabilities worth preserving

| Capability and actual workflow (**C**) | Source owners to reuse |
| --- | --- |
| Enter Layout Mode; choose a page; Add creates an empty landscape A4; Rename/Delete manage names, with last-page protection and selection repair. Orientation is in the row context menu. | [layoutpanel.cpp](../../gui/layoutpanel.cpp): `OnAddLayout`, `OnContextMenu`, `EnsureValidSelection`; `core/layouts/LayoutSelectionPolicy.*` |
| Add 2D View captures the current editor state into a centered frame. Double-click/Edit opens a modal viewer; OK resolves the intended view ID, retaining frame and stacking; Cancel restores temporary state. | [mainwindow_layout.cpp](../../gui/mainwindow_layout.cpp): `OnLayoutAdd2DView`, `BeginLayout2DViewEdit`, `OnLayout2DViewOk/Cancel` |
| Click selects an element; drag moves, handles resize; images retain aspect. Empty-space drag pans, wheel zooms, Z fits, Delete removes; context menus edit/delete/bring front/send back. | [layoutviewerpanel.cpp](../../gui/layoutviewerpanel.cpp): `OnLeftDown/Up`, `OnMouseMove`, `OnRightUp`; `gui/layout_viewer_{interaction_session,selection_state,viewport_state}.*` |
| Five typed element collections have frames, local IDs and zIndex. Legends offer symbol-view choices, visibility/custom names and channel column; event tables hold seven fields; rich text and images have dedicated editors. | [LayoutCollection.h](../../core/layouts/LayoutCollection.h); `gui/layoutviewerpanel_{view,legend,eventtable,text,image}.cpp`; corresponding edit dialogs |
| PSTG stores layout metadata and packaged images; portable `.pslayout` export/import and legacy JSON import exist, including defaults, name collision handling and resource validation. | [LayoutManager.cpp](../../core/layouts/LayoutManager.cpp): `LoadFromConfig`, `SaveToConfig`, template import/export; `LayoutTemplateSerializer.*`, `LayoutTemplatePackageService.*`, `LayoutImageResourceRegistry.*`, `LayoutDefaultsLoader.*` |
| Print Layout validates the selected nonempty page, captures each viewport sequentially, collects supporting elements and exports one PDF. Vector scene primitives, symbol XObjects, fonts, image resources and symbol-usage diagnostics exist. | [mainwindow_print.cpp](../../gui/mainwindow_print.cpp): `OnPrintLayout`; [layout_pdf_exporter.cpp](../../viewer2d/pdf/layout_pdf_exporter.cpp): `ExportLayoutToPdf`; `viewer2d/pdf/layout_pdf_symbol_usage.*` |

Retain current [user workflow](../user/layouts.md), [module ownership](../developer/architecture.md),
[maintenance contract](../developer/code_health.md) and [command contract](../developer/command_api.md).
Caches, capture versions and persisted RGBA previews are acceleration data, not
authoritative page or symbol content; keep their invalidation/resource proofs.

## 3. Main limitations and technical evidence

Priorities: **P0** correctness prerequisite, **P1** essential 1.8.0 workflow,
**P2** subsequent improvement. Block letters refer to section 5.

| Finding / current behavior (**C**) and evidence | Limitation or risk; correction (**D**), priority/dependency |
| --- | --- |
| `gui/layoutpanel.cpp` page add/rename/delete/orientation/import call `LayoutManager` without `PushUndoState`; manager methods synchronize configuration but do not own history. Element add/edit/delete/stacking generally push GUI snapshots. | Page actions cannot promise their own Undo unit. Route both through the same validated document mutation service. **P0/A**. Verify every action and dirty-state transition (**V**). |
| [configservices.cpp](../../core/configservices.cpp): `HistoryManager::Undo/Redo` put `snap.layoutsCollection` into the opposite stack, rather than current layout metadata. [configmanager.cpp](../../core/configmanager.cpp): `Undo/Redo` pass an initially empty layout optional and reload the manager afterward. | Source-level snapshot asymmetry: A→B→Undo→Redo risks restoring A again (**V**). Transfer the departing layout state with the departing scene/overrides; test interleaved edits and missing metadata. **P0/A**. |
| `LayoutViewerPanel::OnLeftDown` pushes history before any movement; `BeginBatchUpdate/EndBatchUpdate` defer serialization. `OnCaptureLost` commits pending movement and discards transient gesture state. | A click/no-op may dirty the project or consume history (**V**); batching is not rollback. Stage frame edits locally, publish once only after a real change; define cancel/capture-loss behavior. **P0/A**, efficiency work **P1/B**. |
| [PageSetup.*](../../core/print/PageSetup.h) defines A3/A4 in mm converted to points. `Layout2DViewFrame` has integer coordinates; `LayoutPanel` exposes only orientation. No direct duplicate-page/element action is wired in these layout paths. | Template export/import is a cumbersome duplication workaround. Add page settings, duplicate and numeric frame editing; preserve existing coordinates during migration. **P1/B**, after A. |
| [viewer2dcommandrenderer.cpp](../../viewer2d/viewer2dcommandrenderer.cpp): `ComputeViewBounds/BuildViewMapping` derive world extents from camera pixels, viewport dimensions and zoom. `OnPrintLayout` independently scales/rounds frames to output dimensions. | A visual zoom is not a declared 1:N scale; resizing/output-size changes lack a user-facing scale lock. Store world center and explicit scale mode; separate paper zoom from drawing scale. **P1/B**, common mapping foundation for C/D/E. |
| [viewer2dstate.cpp](../../viewer2d/viewer2dstate.cpp): `CaptureState/ApplyState`, `ScopedViewer2DState` copy per-view labels and layer/type filters through shared config. `RestoreLayout2DViewState` applies a selected view to that state. | View isolation depends on save/restore and GUI lifetime. Supply explicit immutable presentation inputs to capture/composition. Retain guards while migrating; verify two differently filtered pages and cancel/export restoration (**V**). **P0/A** boundary, **P1/B/D** consumer migration. |
| [fixture_label_overrides.*](../../viewer2d/fixture_label_overrides.h) has UUID-keyed optional show/size/distance/angle settings. [label_render_system.cpp](../../viewer3d/labels/label_render_system.cpp): `BuildFixtureLayoutKey/Entry`, `DrawAllFixtureLabels` resolve three hard-coded fields. Bottom's enum value 3 is clamped to Side index 2 by override resolvers; its anchor projection follows Top. | Individual overrides win over applied viewport defaults, but lack reusable field layouts or viewport-local fixture placement. Bottom's mixed policy needs explicit compatibility handling and runtime characterization (**V**). Extend this resolver/model through a legacy adapter, without a second label engine. **P1/C**, after A/B units. |
| [fixture_symbol_resolution.cpp](../../core/symbols/fixture_symbol_resolution.cpp): `ResolveFixtureSymbolView/SelectView` separates standard exchange and internal rendering. Internal precedence is project override, generated project resource, legacy, then standard; missing internal views can fall back to Top/runtime geometry. | Keep provenance and fallback diagnostics in every consumer. Bottom is Perastage presentation, not a new standard GDTF SVG view. Label styles must reference resolved symbols rather than embed text into SVGs. **P1/C**, preserve current ownership. |
| `gui/layoutlegenditems.cpp::BuildSharedLayoutLegendItems` reads all scene fixtures, groups by type name and chooses a deterministic symbol representative; mixed channel counts suppress the single count. | A legend need not match a filtered viewport or distinguish mode/symbol variants. Add explicit scene/viewport scope and semantic grouping by type/mode/resolved symbol; migrate custom row settings. **P1/D**, B/C inputs. |
| `LayoutDefinition` uses separate per-type vectors; selection stores one `(kind,id)`. GUI update/hit-test/stacking and PDF dispatch enumerate kinds independently. | New annotations multiply branches; no layout multi-selection/alignment/distribution/locking tools in the inspected UI. Share element references, frame/z-order operations and selection geometry while retaining typed payloads. **P1/B**, extensible composition **P1/D/E**. |
| [command_mutation_transaction.cpp](../../core/command/command_mutation_transaction.cpp): transaction captures scene/selection only. `GuiProjectMutationHost::CommitMutation` samples current layout metadata during publication. Existing scene query/CLI surfaces expose no Layout semantic operations. | Scene rollback cannot restore provisional document edits, and late capture is not a document pre-state. Extend the neutral host with a bounded document snapshot; add commands/queries with GUI and CLI consumers. **P0/A**, adapters **P1/B**; no speculative universal object framework. |

### Preview and PDF composition differences

All rows are **C** algorithm/ownership findings; visible severity is **V**.
Detailed font/encoding analysis remains in the earlier audit, section 5.

| Concern | Current paths / consequence | Focused correction (**D**) |
| --- | --- | --- |
| Text | `gui/layouttextutils.cpp::BuildLayoutTextExportData/RenderTextImage` shares extraction, then wx measures/draws preview. PDF `renderText` uses TTF metrics and common baselines; preview top-aligns mixed runs with synthetic line height. Rich-text attributes are only partly retained; neither path supplies full width-based wrapping. | Compose runs, wraps, baselines and overflow once; declare supported rich-text attributes and diagnostics. |
| Fonts/encoding | PDF `pdf_font_metrics.cpp::EncodeWinAnsi` replaces unsupported Unicode with `?`; `pdf_objects.cpp` uses WinAnsi font objects. Preview font selection is independent. Label capture also adjusts line advance by 0.82; PDF legend/table fonts use 1.45 compensation, free text does not. | Pin font identity and shared metrics; add supported Unicode glyph mapping/embedding with extraction mapping. Keep legacy sizing until measured compatibility fixtures justify replacement. |
| Geometry/strokes | Preview replays via `gui/layoutviewerviewrenderer.cpp::RenderLayoutViewCommandBufferToImage`; PDF uses `BuildViewMapping`, flips page Y, and adds screen-matching stroke factors. Captured fallback symbols and SVG/legend fitting have separate transforms. | One page/world mapping and explicit paper stroke units, shared symbol bounds/transforms; round only at raster output. |
| Supporting elements | Preview `_legend.cpp`/`_eventtable.cpp` and PDF `renderLegend`/`renderEventTable` separately calculate columns, text fitting and symbol slots. | Shared measured rows and placement above backend drawing; preserve shared legend aggregation. |
| Clipping/order/alpha | PDF emits viewport/text `re W n` clips outside `CommandBuffer`; preview clips at raster surfaces. Both sort zIndex, but implement ordering separately. Canvas carries RGBA; `pdf_draw_commands.cpp::GraphicsStateCache` emits RGB/width without vector opacity states. | Share rectangular clips and tie order; qualify vector alpha separately from raster image transparency. Compare equal-z overlap before alleging an order defect. |
| Resources/capture | `OnPrintLayout` requires an offscreen wx viewer even for text/image-only pages and captures views sequentially. It snapshots project symbols for the worker, but capture still reads live config/scene. Images can be skipped when `BuildLayoutImageExportData` fails. | Freeze document/scene/resource revisions for export; report missing resources explicitly; do not silently omit content. Retain registries, leases, cache validation and sequential capture until safely isolated. |

### Professional workflow reference points

Official documentation checked on the audit date; **R** means documented,
not personally tested. No claim of complete feature parity or PDF internals.

| Reference | Verified behavior (**R**) | Perastage design inspiration (**D**) |
| --- | --- | --- |
| [Capture 2026 Plot Editor](https://www.capture.se/Manual/en-UK/2026/PlotEditor.html), [2025 Design Tab](https://www.capture.se/Manual/en-UK/2025/DesignTab.html) | Page insets, multi-selection, dimensions/margins, PDF export, dynamic symbol key and underlays; plot styles control annotations. | Fast sheet composition, scoped legend and reusable presentation defaults; full underlay inheritance can wait. |
| [Spotlight label legends](https://app-help.vectorworks.net/2021/eng/VW2021_Guide/LightingDesign1/Setting_up_label_legends.htm), [2026 sheet viewports](https://app-help.vectorworks.net/2026/eng/VW2026_Guide/Viewports1/Creating_sheet_layer_viewports.htm) | Reusable labels display device fields; multiple styles can be selectively assigned. Viewports expose scale, layer/class visibility and drawing labels. | One field-style library plus explicit viewport settings; no need to reproduce the class system or general CAD editor. |
| [WYSIWYG 2026 layout guides](https://cast-soft.com/wysiwyg-2026-layout-guides-for-layouts/) | PRES layout guides align/position elements and remain absent from PDF/print. | Nonprinting guides and alignment tools; vendor page does not establish vector fidelity or label-style semantics. |

[USITT's 2025 announcement](https://www.usitt.org/news/lighting-design-technology-commission-releases-updated-lighting-documentation-recommended)
confirms updated Lighting Documentation Recommended Practice, including modern
fixtures/control graphics. Its linked full guide was unavailable to this audit;
no clause-level compliance is claimed. **D:** offer configurable symbol keys,
field explanations, title/revision information, scale and readable identification.
Support metric and imperial conventions without assigning universal meanings to
circles/hexagons, equating Fixture ID with control channel, or hard-coding US units.

## 4. Recommended target design

### Document model, units and editing

- Keep `LayoutCollection` and typed payloads. Introduce stable page identity with
  deterministic migration; retain `(kind,id)` references scoped to the page.
  Names remain editable display values. Duplicate remaps local references and IDs,
  retains scene UUID bindings and shares immutable image/symbol resources.
- Add common frame, stacking, lock and selection operations with immediate editor
  consumers. Start with a small typed visitor/helper, not a plugin object hierarchy.
  New leader/dimension payloads join the same document and history boundary.
- Keep paper coordinates in points (72/inch), display mm/inches; add fractional
  precision through a versioned serializer. Convert scene millimeters once at the
  projection boundary. At 1:N, `paperPt = sceneMm × 72 / (25.4 × N)`.
  Legacy camera definitions keep their original mapping until explicitly converted.
- Viewport inspector: projection, world center/crop, fit versus locked scale,
  frame/background, visibility and style. Resize at locked scale changes crop;
  fit is an explicit alternative. Sheet zoom/DPI never change physical output.
  Add A-series and US/custom page dimensions, margins, orientation, numeric frames,
  duplicate, nudge, multi-select, align/distribute and nonprinting guides.
- Resolve layer filters by existing layer UUID identity. Legacy template strings
  require explicit name/UUID discrimination and unresolved-token diagnostics;
  portable templates must not silently bind a foreign project's UUID by name.
  Keep fixture-type filters as presentation predicates, not identity substitutes.

### One symbol and dynamic-label presentation system

- Reuse `ProjectFixtureSymbolStore`, `ResolveFixtureSymbolView`, preparation,
  revision-aware SVG caches and symbol snapshots. Preserve authored standard
  Top/Front/Side, project overrides/fallbacks and historical Bottom compatibility;
  use the same resolved definition/view/offsets for viewport, legend and PDF.
- Extend `fixture_label_overrides` into a neutral resolved-presentation input/output
  boundary; its existing APIs become compatibility adapters. Retain UUID remapping,
  reconciliation, optional inheritance and Undo coverage. Never key by fixture name,
  numeric Fixture ID, file path or legend row caption.
- Named styles contain ordered field bindings, formatting, missing-value policy,
  anchor/offset, alignment, border shape and font/size in paper points. Start with
  Name, Fixture ID and DMX; add type/mode/position or user metadata only through
  explicit read-only field resolvers. No expression language is needed for 1.8.0.
- Resolve each property in order: project defaults → viewport style/settings →
  project fixture override → viewport-local fixture override. Unset inherits;
  false/zero remain explicit. Legacy overrides retain their current precedence;
  preserve Bottom's legacy Side-setting lookup until explicitly assigning a new
  Bottom style; do not silently reinterpret existing three-entry arrays.
- Bind values to the current fixture UUID and scene revision, never copied text.
  Paper-sized labels stay readable as drawing scale changes; model-sized legacy
  labels retain their appearance until converted. Add reset-to-inherited and local
  drag/leader placement before considering automatic collision avoidance.

### Shared composition, commands and compatibility

- Core owns document semantics, style resolution, field evaluation and semantic
  commands/queries. Extend the existing viewer2d rendering layer with derived page
  composition: ordered elements, physical transforms, clips, measured text/rows,
  symbol references and resources. Do not introduce a new Core→GUI dependency.
- Preview and PDF consume the same immutable composition. Reuse `CommandBuffer`
  primitives/instances; add only the clip/text capabilities needed by these two
  consumers. Dimensions remain semantic endpoints/references above the buffer,
  then emit lines/arrows/text; never recover dimensions from strokes.
- Snapshot scene/document/style/symbol/resource revisions together. Geometry and
  text caches include those revisions and scale/font identity. Screen culling,
  label budgets and low-resolution cached previews must not truncate final output.
- Use the existing Command Core result/diagnostic/publication contract. Extend
  document pre-state capture and rollback, validate before publishing, serialize
  once and create one Undo entry per real operation. Keep one project history.
  Supply bounded list/get page/element queries and concrete page/viewport/frame
  commands to GUI and CLI; Console/MCP adapt them later, without parsing GUI text.
- Persist annotations/styles in PSTG/layout packages only; MVR remains scene
  exchange authority. No proprietary page annotation becomes authoritative MVR data.
  Keep original GDTF ownership and immutable resources. Evolve the serializer with
  explicit migrations: legacy PSTG/JSON/pslayout inputs must preserve appearance.
  `FromTemplateDocument` currently accepts positive future schema versions; before
  emitting new semantics, reject unsafe future writes or preserve unknown fields,
  rather than read-and-rewrite them destructively. Retain original inputs on failure.

## 5. Prioritized implementation roadmap

Each row is one coherent implementation block, potentially one substantial PR;
split only where review or compatibility evidence requires it.

| Block | Scope and user outcome | Dependencies / exit evidence |
| --- | --- | --- |
| **A — P0: trustworthy document editing** | Repair layout snapshot transfer; unify page/element mutation, dirty state and rollback; explicit document context, basic Core queries/commands. | First. Regression evidence for page/element history, no-ops, failures and legacy load/save. Existing MVR P0 preservation work remains independently required. |
| **B — P1: page and viewport editor** | Page settings/duplicate, precision frames, multi-select/arrange, viewport inspector, UUID filter resolution, physical scale/crop with legacy adapter; GUI and CLI use A's operations. | A; freeze unit/mapping contract early. Calibration at 1:50/1:100, DPI-independent frames and isolated filtered views. |
| **C — P1: unified presentation** | Reusable field-label styles, scoped overrides, paper sizing, label reposition/reset; common resolved symbols and explicit field-key legend. | A + B's units/references. Style/resolver work can proceed alongside B after contract agreement; existing fixtures/Bottom and legacy labels retain appearance. |
| **D — P1: consistent vector publication** | Shared page text/legend/table composition, font/Unicode contract, clips/order/strokes/alpha, resource snapshot and preflight; scope legends to scene or viewport. | A boundary; B mapping and C styles for final integration. Text metrics/font qualification can proceed independently. PDF geometry, glyph extraction and overlap/image checks pass. |
| **E — P1: bounded annotation tools** | Leaders/callouts, straight linear dimensions, scale bar and title/revision fields; scene UUID anchors or explicit page points, paper-sized strokes/text. | A/B and shared text/placement from D. Deleted anchors produce visible unresolved diagnostics; measurements update after scene edits and Undo. No full geometry editor. |

Essential 1.8.0 is A–E within the bounded scope above; trim annotation variety
before compromising history, scale, labels or output fidelity. **P2/deferred:**
shared master-page inheritance, arbitrary grouping/rotation, smart label packing,
advanced dimensions, broad CAD/DXF/DWG interoperability, beam simulation and full
geometry editing. CLI PDF export follows removal of wx capture prerequisites;
do not claim headless readiness merely because semantic editing is exposed.

## 6. Suggested first implementation block and acceptance criteria

Start with **A**, not new element types. Touch focused history/document services
and GUI adapters; do not grow `layoutviewerpanel.cpp` or the PDF hotspot with new
business logic. Retain current storage and resources while proving the boundary.

1. Characterize then repair `HistoryManager::Undo/Redo`: A→B→C→Undo→Undo→Redo→Redo
   restores exact layouts, fixture overrides and symbols, including interleaved
   scene edits. Test absent layout metadata and resource lifetime after page deletion.
2. Route page add/rename/delete/orientation/template import and existing element
   add/edit/frame/delete/stack through one neutral document mutation path. Validate
   names, IDs and frames first; failed/no-op/cancelled actions publish nothing.
3. A real drag commits once; selection/pan/zoom creates no history or dirty change.
   Decide capture-loss semantics explicitly and cover both move and deferred resize.
4. GUI and a minimal CLI page-list/rename operation consume the same service/query;
   results expose stable IDs/diagnostics and metadata change state. Inject publication
   failure to prove document/scene/selection rollback without GUI dependencies.
5. Round-trip synthetic legacy PSTG, JSON and portable packages with text/images,
   all element kinds and UUID overrides; preserve frames/zIndex/resources and last-page
   protection. Existing collection, template, selection and resource tests remain green.
6. Later B–E gates: a 1 m line at 1:50 measures 20 mm on paper; 9 pt labels remain
   9 pt across scales; identical composition baselines/clips agree within 0.25 pt
   between preview geometry and PDF. Verify Unicode extraction, vector symbols,
   transparent overlap and missing-resource diagnostics with synthetic fixtures.

Audit validation: all four mandatory repository guards, `check_docs_links.py`,
`node tools/docs/build_static_docs.cjs --check` and `git diff --cached --check`
passed. The module-tree guard needed `PERASTAGE_TEST_PYTHON` set to the available
Python interpreter. Git fetch was unavailable; current-main verification used
GitHub's API. No full application build or runtime/PDF checks were run; printed
calibration and cross-platform comparison remain implementation acceptance work.
