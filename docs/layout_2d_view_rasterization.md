# Layout 2D view rasterization

Layout preview renders embedded 2D views through an RGBA raster cache layer. The preview cache stores pixels that can be uploaded to OpenGL textures by the layout editor, which keeps the editor responsive while preserving the established Viewer2D rendering path.

PDF export remains separate from this preview texture flow. Exported PDFs must continue to use the existing PDF/layout export code and must not depend on layout preview textures, OpenGL texture IDs, or transient preview raster cache state.

The current `Layout2DViewRasterizer` service wraps the existing `Viewer2DOffscreenRenderer` and `Viewer2DPanel` path. It preserves the command-buffer cache reuse path, persistent RGBA cache reuse, and the fallback `Viewer2DPanel::RenderToRGBA` behavior that the layout preview already used.

Future work may optimize how layout previews are captured or cached, but this layer intentionally does not replace the rendering backend. Do not reintroduce Cairo or add a new Skia, Qt, NanoVG, EGL pbuffer, or FBO-based backend as part of this rasterization boundary.
