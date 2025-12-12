# Print plan options and test guidance

## Current export options
- **Plan Printing preferences (UI)**: Users can choose page **size** (A3 by default), **orientation** (portrait by default), **grid** visibility (on by default), and footprint style (**Detailed** by default). The UI writes these selections into `PlanPrintOptions` before triggering a capture, so the resulting PDF reflects the preferred layout without additional toggles.
- **Stream compression**: `PlanPrintOptions::compressStreams` controls whether page and symbol streams are deflated with zlib before being written. When enabled the exporter emits `/Filter /FlateDecode` on the content streams, cutting the file size and keeping Acrobat/Preview compatibility. See the compressor guardrails around the main and symbol streams in [`viewer2d/planpdfexporter.cpp`](../viewer2d/planpdfexporter.cpp).
- **Simplified fixture footprints (capture-time)**: `PlanPrintOptions::useSimplifiedFootprints` is latched when the frame capture begins so the geometry buffer only records simplified shapes once for the current export. The flag is carried through `Viewer2DPanel::CaptureFrameAsync` into the recording canvas setup before rendering. Refer to [`viewer2d/viewer2dpanel.cpp`](../viewer2d/viewer2dpanel.cpp) for the capture wiring and to [`viewer2d/planpdfexporter.h`](../viewer2d/planpdfexporter.h) for the option definition.
- **XObject reuse for symbols**: Repeated fixtures are stored as PDF Form XObjects: the exporter collects symbol definitions, assigns stable names, and replays placements through `/XObject` references. That keeps identical fixtures from being re-serialized and reduces output size while preserving vector fidelity. The batching happens when building `xObjectNames`/`xObjectIds` and emitting placements in [`viewer2d/planpdfexporter.cpp`](../viewer2d/planpdfexporter.cpp).
- **Grid toggle**: `PlanPrintOptions::printIncludeGrid` reflects the `print_include_grid` configuration and determines whether the capture pipeline includes the 2D grid layer. The flag is passed into `Viewer2DPanel::CaptureFrameAsync`, which forwards it to the controller during recording so the exported plan matches the on-screen grid setting. See [`viewer2d/viewer2dpanel.cpp`](../viewer2d/viewer2dpanel.cpp) and [`gui/mainwindow.cpp`](../gui/mainwindow.cpp).
- **Detailed vs. Schematic footprints**: The **Detailed** option maps to `PlanPrintOptions::useSimplifiedFootprints = false`, rendering full fixture geometry. The **Schematic** option maps to `true`, collapsing fixtures to simplified outlines for faster exports and lighter PDFs.

## Manual test checklist
1. Load a representative scene (any of the sample MVR/PDF assets under `docs/` works) and open the 2D view.
2. Export twice: once with compression enabled (default) and once disabled. Expect the compressed PDF to remain under ~1–2 MB for a one-page plan with tens of fixtures; the uncompressed version should be several times larger but visually identical.
3. Toggle **Print grid** in settings and export again, confirming the grid appears only when requested and that strokes align with the on-screen grid spacing.
4. Verify that repeated fixtures render cleanly without fuzzy edges, confirming XObject reuse keeps vector fidelity.
5. Use zoom/pan offsets in the 2D view before exporting and confirm the PDF framing matches the viewport (no unexpected shifts).

## Automation ideas
- Add a harness that feeds a prerecorded `CommandBuffer` into `ExportPlanToPdf` and asserts the resulting file size falls within an expected range when compression is on versus off (e.g., ratio > 3x for the same buffer).
- Capture a fixture-heavy view using `Viewer2DPanel::CaptureFrameAsync` in a headless test, then parse the PDF catalog to confirm `/XObject` entries exist and are referenced by the page resources.
- Include a pixel-for-pixel comparison by rasterizing the generated PDF (e.g., via Poppler or Ghostscript) and comparing it to a golden PNG to ensure grid toggles and simplified footprints render consistently.
