# Views

Use 3D to check spatial relationships and orientations, and 2D to check plans, positions and readability. Show hidden viewer panes from **View**.

## Navigate and review

In 3D, drag to orbit, Shift-drag or middle-drag to pan, and use the wheel to zoom. Right-click and choose **Render style** for a different presentation, including type, layer or universe coloring.

In 2D, drag empty space to pan and use the wheel to zoom. Select scene elements to cross-check their rows in Fixtures, Trusses, Hoists and Objects. Double-click a cell within a multi-selection to edit that column for the selected rows.

## Move and select

- **Drag Move** enables left-button dragging of scene selections; it is disabled by default.
- **Axis Lock** constrains movement. Disabling it allows free 2D movement or movement on a camera-facing plane in 3D.
- **Local Axes** changes viewport transform axes.
- **Magnet** snaps compatible anchors while moving or inserting elements.
- **Cross-table Actions** allows viewport tools to target object types beyond the active table; it is disabled by default.

Grouped movement follows [Selection & Movement preferences](preferences.md#selection--movement). Review the final position in both views and use Undo if needed.

See [Shortcuts and Command Bar](shortcuts-and-command-bar.md) for keyboard, mouse and Console input. For printable pages, use [Layouts and PDF](layouts.md).

## Fixture labels

Open **Edit Fixture Labels...** in the 2D View settings panel. Choose **Project defaults** or **Selected fixtures**, then select Fixture Name, Fixture ID or DMX Address and Top, Front or Side. The preview uses a resolved project/GDTF symbol when available; a marked placeholder appears otherwise. For multiple fixtures, the preview shows the first fixture and edits affect the whole selection.

Each field supports visibility, normal/bold PerastageSans, fractional scene-metre sizes, color, a symbol-bound anchor, alignment and numeric X/Y offsets. Positive Y moves upward. **Apply** commits one Undo step; **OK** applies and closes; **Cancel** discards edits since the last Apply. **Reset to Default** clears the current scope's overrides: selected fixtures inherit project defaults, while project defaults return to the existing legacy configuration. Apply the reset to keep it.

Opening the editor does not modify settings. Unedited legacy fields retain their existing renderer and saved sizes/positions. The preview adapts their values approximately; editing a field explicitly opts it into shared composition. New composition uses the bundled Noto Sans regular/bold fonts in both 2D and Layout PDF, with fixed scene sizes independent of screen DPI and zoom. It supports English and Western European WinAnsi characters. Unsupported text is visibly marked `[unsupported text]` in both targets; the original fixture attribute is retained. Change that attribute to obtain a complete label. Arbitrary system fonts and viewport-specific styles are not offered.
