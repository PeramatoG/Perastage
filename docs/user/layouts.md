# Layouts and PDF

Layouts arrange scene views and supporting information on printable pages.

## Prepare a page

1. Open **View → Layout Views → Layout Mode View**. Show the **Layouts** panel from **View** if needed.
2. Select a layout, or use **Add** to create one. **Rename** and **Delete** manage pages; the last layout cannot be deleted.
3. Use the layout's context menu to choose A3/A4 paper size and orientation. Changing paper keeps element positions, frame sizes and drawing scales unchanged.
4. Add elements from the Layout toolbar:

| Action | Content |
| --- | --- |
| **Add 2D View** | A 2D scene view |
| **Add Legend** | A fixture legend |
| **Add Event Table** | Event information |
| **Add Text** | Notes and headings |
| **Add Image** | Images such as logos |

Edit each element's available settings to choose content and presentation. Check scene data in the [views and tables](views.md) before producing documentation.

Legends group fixture types across the whole scene. Edit a legend to reorder or hide types, set custom names, choose Bottom/Front/Side symbols, and show or hide the channel column. Enabled Side symbols are included in the vector PDF when a corresponding symbol is available. Oversized symbols are clipped to their column and row; very small frames can hide rows.

## Arrange and edit

Drag a frame to move it and a frame handle to resize it. Double-click a frame to edit the element. Right-click for editing, deletion and stacking controls. Drag empty space to pan and use the wheel to zoom. **Z** fits the layout; **Delete** removes the selected layout element.

Page Add, Rename, Delete, Orientation and template Import share the project's
Undo/Redo history with element editing. Selection clicks and unchanged edits do
not add history. A completed move or resize records one change; **Escape** or
loss of mouse capture cancels the preview without changing the saved document.

Save the `.pstg` project to keep your layouts. **Export template** creates a portable `.pslayout` package including referenced images. **Import template** accepts `.pslayout` and legacy `.json` templates.

## Viewport scale and numeric frame properties

Double-click a 2D view, then choose **Viewport properties**. Frame width and
height use millimeters. Move the element on the Layout page to change its
position. Millimeters display
without decimal places. You can enter fractional values; unchanged fields retain
their full stored precision in projects and templates. Arrow buttons adjust
frame dimensions by 1 mm, world centers by 0.01 m and the scale
denominator by 1. They retain fractional values internally; typing custom values
and choosing a common scale remain available. Frames can be partly outside the page;
only the portion inside the page prints. Frame dimensions must be at least
1 point (about 0.353 mm), with positions and dimensions bounded to 200000 points.

**Free / Variable** is the default for new views, old projects and templates.
Use the existing visual editor to zoom, pan and fit the scene. Resizing retains
the established flexible workflow. The approximate effective free scale, when
available, describes the stored camera and current frame; it is not a lock.
The editor's **Visual framing** slider adjusts visual framing, not a physical ratio.

Choose **Fixed** explicitly for an exact drawing scale. Switching from free
keeps the current view center, including when a previous fixed center was saved.
Select 1:25, 1:50,
1:100, 1:200 or 1:500, or enter a custom denominator between 0.01 and 10000000.
At 1:50, one scene meter occupies 20 mm on paper; at 1:100 it occupies 10 mm.
The stored center uses scene meters: Top X/Y, Front X/Z, Side Y/Z. Side's
horizontal display runs toward negative Y; the numeric center remains world Y.
Change the center numerically or pan directly in the visual editor. In fixed
mode the red rectangle shows the actual crop at the selected scale. A visible
notice identifies the fixed ratio and explains why zoom is locked. Zoom, fit
and the Visual framing slider are locked; dragging and arrow keys move the
center without changing scale. Resizing the editor fits the displayed rectangle
to the window while preserving the world area inside it. Switching back to free
mode restores the previous free framing. Both modes use the ordinary 2D scene
renderer for the Layout preview, preserving the same scene drawing and symbols.

Fixed scale preserves object size and center when moving or resizing the frame;
a larger frame reveals more scene. Layout editor zoom, display DPI and capture
resolution affect screen rasterization, independently of paper geometry.
Switch back to **Free / Variable** to use the preserved free camera. Filters,
labels and presentation settings are retained. Valid changes in **Viewport properties**
preview immediately in the editor, including scale mode, drawing scale, center
and frame size. Incomplete or invalid numbers keep the last valid preview.
**Cancel** (or closing the properties dialog) restores the previous properties
and framing; **OK** keeps the changes in the editor draft. Property edits are drafts until
the outer editor's **OK**; **Cancel** discards them. Accepted changes participate
in Undo/Redo and PSTG/PSLAYOUT persistence.

## Export PDF

Select a non-empty layout and choose **File → Print Layout...**. Choose a writable `.pdf` destination. The PDF uses the selected layout's page size and orientation and includes its supported elements. Review the exported file before printing or sharing.

For missing views, incorrect output or export errors, see [Troubleshooting](troubleshooting.md#layout-and-pdf-problems).

Fixture labels are configured in **2D View settings → Edit Fixture Labels...**. Project defaults and UUID-based fixture overrides also feed Layout viewports and vector PDF. Newly edited fields use the same bundled fonts and positioned composition in both targets. Legacy fields and Layout free text keep their earlier typography. Event Tables share their own point-space composition between preview and PDF. See [Fixture labels](views.md#fixture-labels) for scope, units, Apply/Cancel and supported characters.

Legends use the bundled PerastageSans regular/bold fonts with the same wrapping, row geometry and symbol columns in the editable preview and vector PDF. Screen zoom changes rasterization only. Printing on different paper uniformly fits the composed Legend without recomputing line breaks. Text that exceeds its column is clipped; its full value remains editable. Western European WinAnsi characters are supported. Unsupported glyphs are omitted on both targets while their original text is retained; PDF completion details identify the affected UTF-8 bytes.

### Event Table preview and PDF

Event Tables use the same bundled PerastageSans composition in the Layout Viewer
and selectable vector PDF. Zoom and output paper scale transform the existing
layout; they do not refit its seven fields. Labels and the first value are bold.
Each cell shows one line, using `...` for overflow when it fits. The complete
value remains editable and is saved unchanged. Unsupported glyphs are omitted
consistently in both outputs and recorded in technical diagnostics; full Unicode
shaping is not supported.

### Output paper size

Pages containing a fixed viewport require **100% document-size output**: select
the same paper size as the Layout. A different output paper size is rejected
with an explanation, before writing a PDF. To use a different paper size,
change the Layout paper first; existing fixed scales and element geometry stay
unchanged, while the page boundary changes. Print the PDF at **Actual size /
100%** in the printer driver; Perastage cannot control subsequent driver scaling.

For pages containing only free-scale viewports, PDF printing uniformly fits the
complete original Layout page to the output
paper, anchored at the top left, with unused space at the right or bottom. 2D
views, Legends, Event Tables, free text and images preserve their relative
positions, proportions, overlap and clips. Changing paper does not refit the 2D
camera or change its visible crop. Content outside the original page remains
clipped. Printing keeps the document orientation and does not edit the Layout or
mark the project dirty.

Images retain their document frames; print raster resolution follows final size
at the existing 300 DPI target, subject to pixel limits. Alpha continues to flatten
to white. Free text keeps its existing formatting and PDF typography; this page
mapping correction does not establish free-text preview/PDF typography parity.

Physical scale guarantees scene geometry placement, not typography parity for
all existing presentation features. Existing paper-space text, outline and
symbol styling limitations remain; annotations and broad typography changes
belong to later milestones.
