# Layout 2D view rasterization

Layout preview renders embedded 2D views through an RGBA raster cache layer. The preview cache stores pixels that can be uploaded to OpenGL textures by the layout editor, which keeps the editor responsive while preserving the established Viewer2D rendering path.

PDF export remains separate from this preview texture flow. Exported PDFs must continue to use the existing PDF/layout export code and must not depend on layout preview textures, OpenGL texture IDs, or transient preview raster cache state.

The current `Layout2DViewRasterizer` service wraps the existing `Viewer2DOffscreenRenderer` and `Viewer2DPanel` path. It preserves the command-buffer cache reuse path, persistent RGBA cache reuse, and the fallback `Viewer2DPanel::RenderToRGBA` behavior that the layout preview already used.

## Interactive preview diagnostics

When an embedded 2D view cannot be rasterized or uploaded as a preview texture, the interactive Layout preview now shows a subtle non-printing placeholder inside that view frame. The placeholder says that the 2D view render is unavailable and points maintainers to the diagnostics log.

This placeholder is only part of the editor preview. It does not change successful preview rendering, persistent RGBA cache reuse, command-buffer cache reuse, `Viewer2DPanel::RenderToRGBA`, print preview, or exported PDF output. PDF export remains independent from preview texture failures and must not include the placeholder text.

Future work may optimize how layout previews are captured or cached, but this layer intentionally does not replace the rendering backend. Do not reintroduce Cairo or add a new Skia, Qt, NanoVG, EGL pbuffer, or FBO-based backend as part of this rasterization boundary.
