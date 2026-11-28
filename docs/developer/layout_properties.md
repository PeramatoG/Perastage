# Layout Properties panel

The docked panel consumes a value snapshot and a commit callback. A row carries
an operation ID and a typed control value. It has no project configuration or
Layout dependencies. A future workspace provider can supply its own IDs,
snapshot, and command adapter.

The Layout provider uses the active page name and ordered `LayoutElementRef`
identities. Selection changes and Layout document commits queue one refresh.
The Layout manager revision and current ordered context are checked before
accepting a control event. Replacing the project, changing pages, removing an
element, or updating document history therefore invalidates stale edits even
before the queued repaint. The GL render and mouse-motion paths never query
the panel.

Document edits call the existing Layout manager operations. Element geometry
uses `SetElementFrame` in the neutral document edit service. The operation
validates before publishing one snapshot, retains unedited viewport residuals,
and quantizes integer frames by the existing rules. An unchanged displayed
millimeter value is a no-op. Position-only preview updates reuse existing Layout
rasters. Resizing or editing an element marks its individual raster dirty.

The text inspector reads the same formatted runs used by Layout rendering and
PDF. Inline style changes apply one property to the whole text box. They use
the existing wx rich-text buffer, font catalog control, installed-font property,
and legacy color rules. Mixed formatting, unsupported rich text, and recovery
archives are inspected read-only and use the existing Edit Text transaction.
The text content itself opens in the full editor.

wxAUI owns panel docking. The stable LayoutProperties pane is hidden outside
Layout Mode. Legacy perspectives retain their existing panes and receive the
new pane on the right. A separate user-only Layout perspective and explicit
visibility preference preserve docking and hiding without a project mutation.

## Native verification

`LayoutExportInput` exercises the native controls and the shared rich-text
adapter on all CI platforms. On Linux, run the workspace smoke with the
application's objects and the platform's wxAUI implementation.

```sh
cmake --build build/wsl-x64-debug --target Perastage
xvfb-run -a -s '-screen 0 1600x1000x24' \
  env LIBGL_ALWAYS_SOFTWARE=1 python3 tests/run_viewport_regression_smoke.py \
  build/wsl-x64-debug --source tests/layout_properties_workspace_smoke.cpp
```

In a Debug build on Windows, macOS, or Linux, open Layout Mode and verify the
left page list, central canvas, and right pane at normal and high display
scales. Resize and float Properties, switch to 2D, 3D, and Inspector, and
return. Hide Properties, repeat the switches, reopen it through View, and then
restart to verify persistence. Opening a legacy perspective should keep its
other positions. Select a fixed-scale viewport and press Enter on an unchanged
frame value, then edit a single dimension and check Undo/Redo. Select several
elements, inspect mixed values, and apply a command from the Alignment toolbar.

Select a legacy Text Box with mixed colors and a recovery archive. Verify the
inspector leaves its XML intact, the Layout and PDF render inherited ink in
black, and Edit Text still preserves the original archive. For a uniform box,
change the color to white, apply a font and alignment, and compare Layout
preview, PDF, PSTG reopen, and a portable Layout template round trip.
