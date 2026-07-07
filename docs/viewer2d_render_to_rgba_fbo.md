# Viewer2D RenderToRGBA framebuffer capture

`Viewer2DPanel::RenderToRGBA(...)` captures the interactive Viewer2D scene into an RGBA pixel buffer for Layout preview rasterization and related capture services. The capture path now prefers a real OpenGL framebuffer object on every platform, including Windows.

## Current capture model

- `Viewer2DPanel::RenderToRGBA(...)` resolves the requested framebuffer size, enables the existing offscreen-render flag temporarily, renders into a dedicated framebuffer capture target, and reads pixels from `GL_COLOR_ATTACHMENT0`.
- The framebuffer capture target owns only OpenGL objects: a framebuffer, a color texture attachment, and a depth/stencil renderbuffer. It does not create or own an OpenGL context.
- The capture path preserves temporary Viewer2D state by restoring the offscreen-render flag, the capture framebuffer size override, framebuffer binding, viewport, scissor state, read buffer, and pixel pack alignment after capture.
- If framebuffer creation or completeness checks fail on a driver, `RenderToRGBA(...)` records the FBO diagnostic reason and uses the legacy `GL_BACK` read path as a conservative fallback.

## Local diagnostics

- Manual diagnostic reports include the active or most recent Viewer2D RGBA capture backend: not used, FBO, GL_BACK fallback, or failed.
- The report also includes FBO capture count, fallback count, failure count, the last capture size, and the latest fallback or failure diagnostic when one exists.
- Successful FBO capture is logged with low noise: the first FBO success is recorded, and later backend transitions are recorded without logging every capture.
- Fallback use is recorded with the framebuffer failure or completeness reason, but repeated fallback captures update counters silently instead of producing per-frame log spam.
- Diagnostics remain local to the application and exported reports are never sent automatically.

## Architecture boundaries

This phase intentionally keeps the existing higher-level offscreen preview architecture intact:

- `Viewer2DOffscreenRenderer` still owns the hidden/off-screen wx host used for Viewer2D-based capture.
- Layout preview ownership remains in `LayoutViewerPanel`, including OpenGL preview textures, PBO upload state, persistent RGBA cache, failure diagnostics, frame drawing, selection handles, and preview placeholders.
- PDF, export, and print rendering remain separate from the interactive Layout preview texture and failure-diagnostic path.
- Layout preview capture continues to use the scoped temporary Viewer2D state rules documented in `docs/viewer2d_state_ownership.md`.

## Temporary fallback

The `GL_BACK` path is a temporary fallback for conservative compatibility while the shared framebuffer capture path is tested across drivers and platforms. It is isolated in a named fallback helper so a future phase can remove it without changing the default capture flow.

## Future phase

A later phase may reduce the dependency on `Viewer2DOffscreenRenderer` and the hidden wx host. That work is out of scope for this framebuffer unification step and should be planned separately from Layout preview scheduling, command-buffer capture, PDF/export/print, and preview texture ownership.
