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

// Builds the loading overlay label shown during layout rendering.
wxString BuildLoadingOverlayLabel() {
  return wxString::FromUTF8("Loading layout...");
}

// Builds the context-menu label for moving an element to the top Z order.
wxString BuildBringToFrontMenuLabel() {
  return wxString::FromUTF8("Bring to Front");
}

// Builds the context-menu label for moving an element to the bottom Z order.
wxString BuildSendToBackMenuLabel() {
  return wxString::FromUTF8("Send to Back");
}

// Builds the context-menu label for opening the 2D view editor.
wxString BuildEditViewMenuLabel() {
  return wxString::FromUTF8("2D View Editor");
}

// Builds the context-menu label for toggling element border visibility.
wxString BuildShowBorderMenuLabel() {
  return wxString::FromUTF8("Show Border");
}

// Builds the context-menu label for deleting a 2D view.
wxString BuildDeleteViewMenuLabel() {
  return wxString::FromUTF8("Delete 2D View");
}

// Builds the context-menu label for editing a legend.
wxString BuildEditLegendMenuLabel() {
  return wxString::FromUTF8("Edit Legend");
}

// Builds the context-menu label for deleting a legend.
wxString BuildDeleteLegendMenuLabel() {
  return wxString::FromUTF8("Delete Legend");
}

// Builds the context-menu label for editing an event table.
wxString BuildEditEventTableMenuLabel() {
  return wxString::FromUTF8("Edit Event Table");
}

// Builds the context-menu label for deleting an event table.
wxString BuildDeleteEventTableMenuLabel() {
  return wxString::FromUTF8("Delete Event Table");
}

// Builds the context-menu label for editing a text element.
wxString BuildEditTextMenuLabel() {
  return wxString::FromUTF8("Edit Text");
}

// Builds the context-menu label for toggling transparent text background.
wxString BuildTransparentBackgroundMenuLabel() {
  return wxString::FromUTF8("Transparent Background");
}

// Builds the context-menu label for deleting a text element.
wxString BuildDeleteTextMenuLabel() {
  return wxString::FromUTF8("Delete Text");
}

// Builds the context-menu label for changing an image element.
wxString BuildChangeImageMenuLabel() {
  return wxString::FromUTF8("Change Image");
}

// Builds the context-menu label for deleting an image element.
wxString BuildDeleteImageMenuLabel() {
  return wxString::FromUTF8("Delete Image");
}

} // namespace layoutviewerpanel
