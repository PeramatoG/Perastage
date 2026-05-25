#pragma once

#include <wx/event.h>
#include <wx/string.h>

class wxWindow;

wxDECLARE_EVENT(EVT_LAYOUT_RENDER_STATUS, wxCommandEvent);

namespace gui::layoutstatus {

// Posts a layout-render status event with text to the provided top-level window.
void PostLayoutRenderStatus(wxWindow *eventSource, wxWindow *targetWindow,
                            const wxString &statusText);

// Pumps pending UI events at a throttled cadence so status-bar progress remains visible.
void PumpLayoutRenderStatusUi();

} // namespace gui::layoutstatus
