#pragma once

class wxWindow;

struct RenderSize {
  int width = 0;
  int height = 0;
  const char *source = "";

  bool IsValid() const { return width > 0 && height > 0; }
};

RenderSize ResolveRenderSize(wxWindow *window);

void ValidateRenderSizeContract(const char *panelName, unsigned long long frameId,
                                const RenderSize &resolvedSize,
                                const RenderSize &viewportSize,
                                const RenderSize &projectionSize,
                                const RenderSize *auxiliarySize = nullptr);
