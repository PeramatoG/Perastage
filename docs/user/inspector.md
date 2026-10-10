# MVR / GDTF Inspector

Open **View → Layout Views → MVR / GDTF Inspector** to examine the current project's snapshot. Inspection is read-only: it does not modify the project or the inspected file, change project selection, or mark the project as edited.

## Choose what to inspect

- The current-project view is a snapshot, not a continuously updating scene. Use **Refresh** after editing to capture the latest project state.
- Use **Open external...** to inspect an external `.mvr` or `.gdtf` without importing it.

## Browse MVR and GDTF content

For MVR, navigate the package tree to inspect archive entries, or use the scene tree to examine scene objects and their names, types and UUIDs. Activate an embedded GDTF to open its fixture definition, then return to the parent MVR.

For GDTF, review FixtureType identity, physical information and revisions. Select a DMX mode to inspect its channels, functions and sets. Review wheels, slots, filters and resource references in the detail pages.

## Source and preview

Select a package resource to examine its source or preview. XML and plain text appear in **Source**; SVG offers XML and a rendered preview. Supported images and GLB/3DS models appear in **Preview**. Unsupported or oversized resources remain listed with a preview status.

Use source search, folding and copy controls to examine XML. Large documents may start with a bounded view and offer an explicit action to load the complete text. Drag pane dividers to give navigation, source, details or preview more room.

## Issues and diagnostics

**Issues** groups reported problems; **Diagnostics** provides more detail for investigation. A preview limitation alone does not mean the file is invalid. The Inspector does not repair packages or download missing resources. Use the normal editing workflows for corrections, or [export a diagnostic report](troubleshooting.md#logs-and-diagnostic-reports) when reporting a problem.
