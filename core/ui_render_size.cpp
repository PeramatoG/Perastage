#include "ui_render_size.h"

#include <wx/window.h>

#include <wx/log.h>

#include <algorithm>
#include <cmath>
#include <limits>

// Resolves the GL framebuffer size in physical pixels for the given window.
RenderSize ResolveRenderSize(wxWindow *window) {
  if (window == nullptr) {
    return RenderSize{0, 0, "null-window"};
  }

  const wxSize logicalClientSize = window->GetClientSize();
  const int logicalWidth = std::max(0, logicalClientSize.GetWidth());
  const int logicalHeight = std::max(0, logicalClientSize.GetHeight());

  const wxSize physicalClientSize = window->ToPhys(logicalClientSize);
  const int physicalWidth = std::max(0, physicalClientSize.GetWidth());
  const int physicalHeight = std::max(0, physicalClientSize.GetHeight());
  if (physicalWidth > 0 && physicalHeight > 0) {
    return RenderSize{physicalWidth, physicalHeight, "window-to-phys"};
  }

  const double contentScale =
      static_cast<double>(window->GetContentScaleFactor());
  if (!std::isfinite(contentScale) || contentScale <= 0.0) {
    return RenderSize{0, 0, "invalid-content-scale"};
  }
  const long roundedWidth =
      std::lround(static_cast<double>(logicalWidth) * contentScale);
  const long roundedHeight =
      std::lround(static_cast<double>(logicalHeight) * contentScale);
  const long clampedWidth = std::clamp(
      roundedWidth, 0L, static_cast<long>(std::numeric_limits<int>::max()));
  const long clampedHeight = std::clamp(
      roundedHeight, 0L, static_cast<long>(std::numeric_limits<int>::max()));
  return RenderSize{static_cast<int>(clampedWidth), static_cast<int>(clampedHeight),
                    "window-content-scale-fallback"};
}

// Validates that all render stages use a consistent framebuffer-size contract.
void ValidateRenderSizeContract(const char *panelName, unsigned long long frameId,
                                const RenderSize &resolvedSize,
                                const RenderSize &viewportSize,
                                const RenderSize &projectionSize,
                                const RenderSize *auxiliarySize) {
#ifndef NDEBUG
  const bool viewportMatches =
      viewportSize.width == resolvedSize.width &&
      viewportSize.height == resolvedSize.height;
  const bool projectionMatches =
      projectionSize.width == resolvedSize.width &&
      projectionSize.height == resolvedSize.height;
  bool auxiliaryMatches = true;
  if (auxiliarySize != nullptr) {
    auxiliaryMatches = auxiliarySize->width == resolvedSize.width &&
                       auxiliarySize->height == resolvedSize.height;
  }

  if (viewportMatches && projectionMatches && auxiliaryMatches) {
    return;
  }

  if (auxiliarySize != nullptr) {
    wxLogTrace(
        "render_size_contract",
        "%s frame=%llu size mismatch resolved=%dx%d(%s) viewport=%dx%d(%s) "
        "projection=%dx%d(%s) auxiliary=%dx%d(%s)",
        panelName != nullptr ? panelName : "unknown-panel", frameId,
        resolvedSize.width, resolvedSize.height,
        resolvedSize.source != nullptr ? resolvedSize.source : "",
        viewportSize.width, viewportSize.height,
        viewportSize.source != nullptr ? viewportSize.source : "",
        projectionSize.width, projectionSize.height,
        projectionSize.source != nullptr ? projectionSize.source : "",
        auxiliarySize->width, auxiliarySize->height,
        auxiliarySize->source != nullptr ? auxiliarySize->source : "");
  } else {
    wxLogTrace(
        "render_size_contract",
        "%s frame=%llu size mismatch resolved=%dx%d(%s) viewport=%dx%d(%s) "
        "projection=%dx%d(%s)",
        panelName != nullptr ? panelName : "unknown-panel", frameId,
        resolvedSize.width, resolvedSize.height,
        resolvedSize.source != nullptr ? resolvedSize.source : "",
        viewportSize.width, viewportSize.height,
        viewportSize.source != nullptr ? viewportSize.source : "",
        projectionSize.width, projectionSize.height,
        projectionSize.source != nullptr ? projectionSize.source : "");
  }
#else
  (void)panelName;
  (void)frameId;
  (void)resolvedSize;
  (void)viewportSize;
  (void)projectionSize;
  (void)auxiliarySize;
#endif
}
