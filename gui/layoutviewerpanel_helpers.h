#pragma once

#include <wx/gdicmn.h>

class wxMouseEvent;
class wxWindow;

namespace layoutviewerpanel {

// Returns the logical client size for a window or zero size when the window is null.
wxSize GetLogicalClientSize(const wxWindow *window);

// Returns the mouse position in logical client coordinates.
wxPoint GetLogicalMousePosition(const wxMouseEvent &event);

// Converts a logical point to framebuffer coordinates using the window content scale.
wxPoint ToFramebufferPoint(wxWindow *window, const wxPoint &logicalPoint);

// Snaps an integer coordinate to the nearest layout grid increment.
int SnapToGrid(int value);

} // namespace layoutviewerpanel
