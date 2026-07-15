# Viewer2D Rendering and Capture Notes

This note collects the active Viewer2D rendering contracts that are useful for maintenance. It covers layout rasterization, render-to-RGBA framebuffer capture, and state ownership during export-style captures.

## Layout 2D view rasterization

The layout view should rasterize only through Viewer2D-owned rendering paths. Keep scene data, capture state, and UI concerns separated so PDF export, preview capture, and on-screen drawing do not depend on each other's transient state.

## Render-to-RGBA framebuffer capture

Render-to-RGBA capture is used when Viewer2D content must be converted into an image buffer for export or preview flows. Capture code should validate framebuffer dimensions, keep ownership local to the capture operation, and report failures instead of returning incomplete image data.

## State ownership during capture

Viewer2D capture may temporarily adjust view or print settings, but those changes must remain scoped to the capture operation and must not leak back into normal interactive state. Long-running exports should snapshot the required options before capture starts.

## Related files

- `viewer2d/` owns 2D rendering, capture, and PDF export behavior.
- `viewer_common/` owns rendering utilities shared across viewers.
- `gui/` owns menu actions and dialogs that initiate capture/export workflows.
