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

Click a frame to select it. **Ctrl + click** toggles an element and **Shift + click**
adds one. **Shift + drag** on empty page space adds every intersecting frame;
marquee additions follow back-to-front stacking order, with ties ordered by
2D View, Legend, Event Table, Text, Image and then their stored order. Overlapping
frames are picked from the front. The newest remaining selection is the primary
(darker outline). Removing and selecting an element again makes it the newest.
Click empty space to clear selection; an ordinary empty-space drag still pans.
Selection is independent of scene selection and clears when switching pages.

Drag any selected frame to move the whole selection. Relative positions and frame
sizes remain unchanged. A single frame still has resize handles; a multiple
selection has outlines and a collective boundary, without resize handles.
Double-click a specific frame to edit only that element. Right-click a single
frame for editing, deletion and stacking controls; a multiple selection offers
batch deletion. **Delete** removes all selected elements in one Undo step.
Use the wheel to zoom and **Z** to fit the layout.

The shared **Alignment** toolbar works on mixed Layout selections of two or more
elements. Choose **Extents**, **First selected**, or **Last selected**, then
**Align Top**, **Align Bottom**, **Align Left**, **Align Right**,
**Center Horizontally**, or **Center Vertically**. First and Last follow selection
chronology and keep that reference frame stationary. Geometric bounds use complete
page frames; insertion points mean the top-left page origins, never fixture
insertion points inside a viewport. Work-plane buttons are disabled in Layout Mode.

Viewports retain fractional point precision. Other elements use whole points:
alignment rounds their target positions to the nearest point (half-point ties
toward positive infinity), with at most 0.5 pt error per axis. A mixed group drag rounds its
common delta to whole points so all relative positions remain exact. Extents
alignment of selections containing integer frames stops within 0.5 pt, avoiding
drift on repeated clicks. Pure viewport alignment retains
fractional precision. Alignment and page movement preserve viewport scales,
cameras, filters and scene positions; selection decorations never print.

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

Choose **Fixed** explicitly for an exact drawing scale. Switching modes
keeps the current view center in both directions. Panning or numeric center edits
in fixed mode remain in place when the scale lock is released.
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
mode restores the previous free zoom while retaining the latest view center. Both modes use the ordinary 2D scene
renderer for the Layout preview, preserving the same scene drawing and symbols.

Fixed scale preserves object size and center when moving or resizing the frame;
a larger frame reveals more scene. Layout editor zoom, display DPI and capture
resolution affect screen rasterization, independently of paper geometry.
Switch back to **Free / Variable** to use the preserved free zoom at the latest center. Filters,
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

Fixture labels use the selected face in the Fixture Label Editor and in 2D and
Layout viewports. PDF uses the same face when it can embed it. Project defaults and fixture overrides feed Layout
viewports. See [Fixture labels](views.md#fixture-labels) for scope and units.

Use **Font family** when editing a legend or Event Table. The selection applies
to that element and preserves its Regular and Bold roles. For free text, select
a span to apply a family to that span or leave the selection empty to apply it
to the whole element. Each text element can use different fonts. PerastageSans,
Noto Serif and Source Sans 3 have genuine italic and bold italic faces. Noto
Sans Mono has Regular and Bold. Use **Document preview** in the text editor
to see the shared Layout rendering. The native editing control can display a
different system rendering while you edit.

When a legacy text element contains formatting the editor cannot round-trip,
the editor keeps a recovery copy of the original alongside your edits and
shows a notice. The Layout and PDF use the supported formatting in the
edited text.

### Event Table preview and PDF

Layout and PDF use the same font advances and baselines. Zoom and output paper
scale transform the composition. Legends wrap group names within their columns.
Event Table cells display one line and use an ellipsis for overflow when it
fits. The complete source remains editable in both cases.

### Installed fonts and portability

The font picker lists the included families first and marks families from your
computer as **installed**. Leave a project on the included families when it
must look the same on machines with different installed fonts. The picker
reports unavailable, unsupported and PDF-substituted faces in its tooltip.
Fixture labels offer Bold and Italic. If the requested style is missing,
Perastage uses a real compatible face and reports the substitution.

Projects store the selection and keep it when a font is missing. They do not
carry installed font files. Use **Refresh fonts** in a font editor after
installing or removing a font, or select a different family. The same saved
selection can recover after the matching face becomes available.

Perastage can render compatible static TrueType and OpenType/CFF fonts locally,
including individual faces from supported collections. PDF can embed a supported
standalone TrueType face when its metadata permits it. It uses an approved
fallback for restricted, preview-and-print-only, bitmap-only or unsupported
faces and reports the substitutions after export. Different fallback advances
can change line breaks. Font metadata cannot confirm every license term.
Check the license for the intended use and distribution.

The bundled fallback covers Spanish accents, Greek, Cyrillic, and common
Arabic and Hebrew text. PDF preserves supported shaped text for selection and
searching. Missing glyphs are reported in the export details.

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

## Reusable fixture label styles

Right-click a 2D viewport frame and choose **Fixture label settings...**. Select
**Inherit project presentation** or a named project style. **Create style from
current settings...** captures reusable field settings; named styles can be edited,
renamed or duplicated. Creating a style moves shared viewport fields into the
profile; fixture-local adjustments remain local. The dialog indicates active local
adjustments. Editing a named style updates every viewport using it,
including other pages. Referenced styles cannot be deleted; restore inheritance
in their referencing views first.

**Customize current viewport...** opens the existing Fixture Label Editor with
explicit **Current Layout viewport** and **Selected fixtures in current Layout
viewport** scopes. In the **2D View Editor**, click a fixture to select its
labels; Shift-click adds or removes a fixture. The selected fixtures are outlined,
and **Edit Fixture Labels...** opens their local scope. Dragging still pans the
camera and cannot move scene fixtures. This selection does not alter the project
selection or create Undo entries. For the frame context-menu workflow, select
fixtures in the ordinary scene before opening label settings. The same preview, background selector and field dragging
remain available. Only the edited orientation and field become explicit. Each
field's visibility, font, ink, outline, anchor, alignment and offsets inherit as
one unit; changing a field copies its current properties into that scope.
**Reset to Default** in this editor clears only that scope's local settings.
**Restore inherited settings** in the viewport dialog clears its profile assignment
and all local adjustments. Neither operation removes project fixture overrides.

Precedence, highest first: viewport-local fixture field, customized viewport field,
named profile field, project fixture override, project default field, legacy
configuration. Missing fields inherit. A local field stays independent of later
profile edits until reset. Profiles cover Fixture Name, Fixture ID and DMX Address
for Top, Front and Side. Bottom retains its historical Side style-key mapping
and Top-plane placement; it is not an additional standard GDTF SVG orientation.

The nested editor's **Apply** keeps the preview draft; **OK** accepts it into the
outer dialog. The outer **OK** publishes one Undo operation; **Cancel** discards
all of its changes. Opening settings, selecting a style or changing preview
background does not save or dirty the project. Camera/scale edits use the same
local label scope when opened from the Layout camera editor.

Profiles and local adjustments are project presentation metadata in PSTG.
Portable PSLAYOUT files include referenced profile definitions. Imports reuse an
identical ID/definition or deterministically remap a conflicting ID. Missing
profiles reject the operation with diagnostics instead of silently changing the
appearance. MVR scene data and external GDTF archives are unchanged.

Sizes, outlines and offsets remain in **scene metres**, including in vector PDF.
A label therefore prints at different sizes at 1:100 and 1:25. Editing a label
does not alter drawing scales, camera framing, scene transforms or symbols.
Paper-sized typography and broader Unicode support remain separate work.
