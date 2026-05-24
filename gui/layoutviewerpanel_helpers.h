#pragma once

#include <wx/gdicmn.h>
#include <wx/string.h>

#include "layoutviewerpanel.h"

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


// Returns whether the selected element type supports Z-order context-menu commands.
bool SupportsZOrderMenuCommands(LayoutViewerPanel::SelectedElementType type);

// Returns the localized loading overlay message shown while layout content is rendering.
wxString BuildLoadingOverlayLabel();

// Returns the shared context-menu label for sending an element to the top Z order.
wxString BuildBringToFrontMenuLabel();

// Returns the shared context-menu label for sending an element to the bottom Z order.
wxString BuildSendToBackMenuLabel();

// Returns the context-menu label for opening the 2D view editor.
wxString BuildEditViewMenuLabel();

// Returns the context-menu label for toggling element border visibility.
wxString BuildShowBorderMenuLabel();

// Returns the context-menu label for deleting a 2D view.
wxString BuildDeleteViewMenuLabel();

// Returns the context-menu label for editing a legend.
wxString BuildEditLegendMenuLabel();

// Returns the context-menu label for deleting a legend.
wxString BuildDeleteLegendMenuLabel();

// Returns the context-menu label for editing an event table.
wxString BuildEditEventTableMenuLabel();

// Returns the context-menu label for deleting an event table.
wxString BuildDeleteEventTableMenuLabel();

// Returns the context-menu label for editing a text element.
wxString BuildEditTextMenuLabel();

// Returns the context-menu label for toggling transparent text background.
wxString BuildTransparentBackgroundMenuLabel();

// Returns the context-menu label for deleting a text element.
wxString BuildDeleteTextMenuLabel();

// Returns the context-menu label for changing an image element.
wxString BuildChangeImageMenuLabel();

// Returns the context-menu label for deleting an image element.
wxString BuildDeleteImageMenuLabel();

} // namespace layoutviewerpanel
