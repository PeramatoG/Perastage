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
RenderSize ResolveRenderSize(wxWindow *window);

void ValidateRenderSizeContract(const char *panelName, unsigned long long frameId,
                                const RenderSize &resolvedSize,
                                const RenderSize &viewportSize,
                                const RenderSize &projectionSize,
                                const RenderSize *auxiliarySize = nullptr);
