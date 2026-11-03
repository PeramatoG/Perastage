# Layouts and PDF

Layouts arrange scene views and supporting information on printable pages.

## Prepare a page

1. Open **View → Layout Views → Layout Mode View**. Show the **Layouts** panel from **View** if needed.
2. Select a layout, or use **Add** to create one. **Rename** and **Delete** manage pages; the last layout cannot be deleted.
3. Use the layout's context menu to choose orientation.
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

## Export PDF

Select a non-empty layout and choose **File → Print Layout...**. Choose a writable `.pdf` destination. The PDF uses the selected layout's page size and orientation and includes its supported elements. Review the exported file before printing or sharing.

For missing views, incorrect output or export errors, see [Troubleshooting](troubleshooting.md#layout-and-pdf-problems).

Fixture labels are configured in **2D View settings → Edit Fixture Labels...**. Project defaults and UUID-based fixture overrides also feed Layout viewports and vector PDF. Newly edited fields use the same bundled fonts and positioned composition in both targets. Legacy fields, Layout free text and event tables keep their earlier typography. See [Fixture labels](views.md#fixture-labels) for scope, units, Apply/Cancel and supported characters.

Legends use the bundled PerastageSans regular/bold fonts with the same wrapping, row geometry and symbol columns in the editable preview and vector PDF. Screen zoom changes rasterization only. Printing on different paper uniformly fits the composed Legend without recomputing line breaks. Text that exceeds its column is clipped; its full value remains editable. Western European WinAnsi characters are supported. Unsupported glyphs are omitted on both targets while their original text is retained; PDF completion details identify the affected UTF-8 bytes.
