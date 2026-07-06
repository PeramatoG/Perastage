# Layout 2D view rasterization

Layout preview renders embedded 2D views through an RGBA raster cache layer. The preview cache stores pixels that can be uploaded to OpenGL textures by the layout editor, which keeps the editor responsive while preserving the established Viewer2D rendering path.

PDF export remains separate from this preview texture flow. Exported PDFs must continue to use the existing PDF/layout export code and must not depend on layout preview textures, OpenGL texture IDs, or transient preview raster cache state.

The current `Layout2DViewRasterizer` service wraps the existing `Viewer2DOffscreenRenderer` and `Viewer2DPanel` path. It preserves the command-buffer cache reuse path, persistent RGBA cache reuse, and the fallback `Viewer2DPanel::RenderToRGBA` behavior that the layout preview already used.

## Interactive preview responsibility boundaries

The interactive Layout 2D preview path is split across focused responsibilities:

- `Layout2DViewCaptureService` schedules and applies command-buffer captures for embedded Layout 2D views. It decides when a capture is stale, applies the Layout 2D view definition to the capture panel, invokes the existing `Viewer2DPanel::CaptureFrameNow` path, and updates capture metadata through the owning panel.
- `Layout2DViewRasterizer` converts cached or captured preview data into RGBA pixels. It remains responsible for command-buffer rasterization, persistent RGBA reuse, and the existing `Viewer2DPanel::RenderToRGBA` fallback.
- `LayoutViewerPanel` owns the Layout view cache, OpenGL texture IDs and upload lifecycle, frame drawing, selection handles, persistent preview state, and interactive failure placeholders.

PDF export, print preview, and printing remain separate from the interactive preview texture path. They must not depend on preview OpenGL textures, transient preview capture state, or the non-printing interactive failure placeholder.

## Interactive preview diagnostics

When an embedded 2D view cannot be rasterized or uploaded as a preview texture, the interactive Layout preview now shows a subtle non-printing placeholder inside that view frame. The placeholder says that the 2D view render is unavailable and points maintainers to the diagnostics log.

This placeholder is only part of the editor preview. It does not change successful preview rendering, persistent RGBA cache reuse, command-buffer cache reuse, `Viewer2DPanel::RenderToRGBA`, print preview, or exported PDF output. PDF export remains independent from preview texture failures and must not include the placeholder text.

Future work may optimize how layout previews are captured or cached, but this layer intentionally does not replace the rendering backend. Do not reintroduce Cairo or add a new Skia, Qt, NanoVG, EGL pbuffer, or FBO-based backend as part of this rasterization boundary.
