# Views

Use 3D to check spatial relationships and orientations, and 2D to check plans, positions and readability. Show hidden viewer panes from **View**.

## Navigate and review

In 3D, drag to orbit, Shift-drag or middle-drag to pan, and use the wheel to zoom. Right-click and choose **Render style** for a different presentation, including type, layer or universe coloring.

In 2D, drag empty space to pan and use the wheel to zoom. Select scene elements to cross-check their rows in Fixtures, Trusses, Hoists and Objects. Double-click a cell within a multi-selection to edit that column for the selected rows.

## Position Tool

Open **View → Position Tool** in either the 2D or 3D workspace. Dock, float, resize or hide the same panel; its placement and visibility are saved with the workspace. With no selection, the coordinate fields are disabled.

The panel is shown by default below Summary in the right sidebar in 2D and 3D. The arrows beside each axis move every effective target by 1 mm per click in metric units or 0.01 ft in imperial units, preserving mixed values and stored precision. Each click is undoable.

X, Y and Z are absolute world coordinates of the insertion point. Edit one field and press Enter or leave the field to apply it; the other axes, rotation and scale stay unchanged. Escape cancels an uncommitted edit. Each completed edit is one Undo step. Invalid or empty input does not move anything.

For multiple selections, shared coordinates appear normally and mixed coordinates are empty. Entering a value assigns that absolute coordinate to every effective target independently. Grouped children follow **Selection & Movement** preferences.

Small red squares mark the effective insertion points in the interactive 2D viewer in Top, Bottom, Front and Side views. They stay the same screen size and are excluded from captures, printed layouts and exports. There are no markers in 3D.

Distance units follow **Preferences → Units**: metres with three decimals, or the existing imperial feet format. Unit suffixes and feet/inches input are accepted. Display rounding does not change stored positions.

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

Each field supports visibility, normal/bold PerastageSans, fractional scene-metre sizes, color, a symbol-bound anchor, alignment and numeric X/Y offsets. Each field has an independent outline switch, color and thickness in scene metres; preview and viewer use a continuous vector halo around the bundled font fill, while PDF traces the embedded glyph. New styles default to a 0.004 m radius. On screen this maps to a one-framebuffer-pixel halo, independent of zoom; larger values proportionally increase its screen weight. The editor preview uses the same pixel policy, including high-DPI conversion. PDF keeps the authored physical radius in scene metres. Saved thickness values are retained. Fixture names keep the traditional break after every two space-delimited words, including after moving or styling the Name field; this compatibility rule is currently fixed. Screen fill uses the same font rasterization as traditional labels so thin letters keep a comparable fill/halo balance when zooming out. Changing only an ink property preserves the displayed position and the full stored offset precision. Positive Y moves upward. Drag the selected text in the preview to position it; Escape cancels the gesture. The preview scale stays fixed while editing; changing view/scope, resizing or resetting refits the preview. White/dark backgrounds affect only the preview, with display-only light contours on dark backgrounds. **Apply** updates the visible 2D viewport while the dialog stays open and commits one Undo step; **OK** applies and closes; **Cancel** discards edits since the last Apply. **Reset to Default** clears the current scope's overrides: selected fixtures inherit project defaults, while project defaults return to the existing legacy configuration. Apply the reset to keep it.

Opening the editor does not modify settings. Unedited legacy fields retain their existing renderer and saved sizes/positions. The preview adapts their values approximately; editing a field explicitly opts it into shared composition. New composition uses the bundled Noto Sans regular/bold fonts in both 2D and Layout PDF, with fixed scene sizes independent of screen DPI and zoom. It supports English and Western European WinAnsi characters. Unsupported text is visibly marked `[unsupported text]` in both targets; the original fixture attribute is retained. Change that attribute to obtain a complete label. Arbitrary system fonts and viewport-specific styles are not offered.
