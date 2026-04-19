#pragma once

class wxWindow;

// Render size used by OpenGL paths. Dimensions are framebuffer pixels
// (physical pixels), not logical/window client coordinates.
struct RenderSize {
  int width = 0;
  int height = 0;
  // Trace/debug string describing the framebuffer-size source.
  const char *source = "";

  bool IsValid() const { return width > 0 && height > 0; }
};

// Resolves framebuffer dimensions (physical pixels) for a wxWindow.
//
// Picking convention:
// - Mouse events arrive in logical client coordinates (DIP).
// - Any code that calls screen-space picking helpers (Get*LabelAt,
//   Get*InScreenRect, overlays drawn in framebuffer space) must convert those
//   logical coordinates to framebuffer pixels first, then use the size from
//   ResolveRenderSize().
// Keeping this convention explicit avoids DPI regressions where logical mouse
// coordinates are mixed with physical render dimensions.
RenderSize ResolveRenderSize(wxWindow *window);

void ValidateRenderSizeContract(const char *panelName, unsigned long long frameId,
                                const RenderSize &resolvedSize,
                                const RenderSize &viewportSize,
                                const RenderSize &projectionSize,
                                const RenderSize *auxiliarySize = nullptr);
