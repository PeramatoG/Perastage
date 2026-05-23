#include "layoutviewerpanel_helpers.h"

#include <cmath>
#include <wx/event.h>
#include <wx/window.h>

namespace {
constexpr int kLayoutGridStep = 5;
}

namespace layoutviewerpanel {

// Returns the logical client size for a window or zero size when the window is null.
wxSize GetLogicalClientSize(const wxWindow *window) {
  if (window == nullptr) {
    return wxSize(0, 0);
  }
  return window->GetClientSize();
}

// Returns the mouse position in logical client coordinates.
wxPoint GetLogicalMousePosition(const wxMouseEvent &event) {
  return event.GetPosition();
}

// Converts a logical point to framebuffer coordinates using the window content scale.
wxPoint ToFramebufferPoint(wxWindow *window, const wxPoint &logicalPoint) {
  if (window == nullptr) {
    return wxPoint(0, 0);
  }
  const double contentScale =
      static_cast<double>(window->GetContentScaleFactor());
  if (!std::isfinite(contentScale) || contentScale <= 0.0) {
    return wxPoint(0, 0);
  }
  return wxPoint(static_cast<int>(std::lround(logicalPoint.x * contentScale)),
                 static_cast<int>(std::lround(logicalPoint.y * contentScale)));
}

// Snaps an integer coordinate to the nearest layout grid increment.
int SnapToGrid(int value) {
  if (kLayoutGridStep <= 1)
    return value;
  return static_cast<int>(
      std::lround(static_cast<double>(value) / kLayoutGridStep) *
      kLayoutGridStep);
}

} // namespace layoutviewerpanel
