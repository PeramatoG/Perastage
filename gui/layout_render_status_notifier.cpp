#include "layout_render_status_notifier.h"

#include <wx/window.h>

wxDEFINE_EVENT(EVT_LAYOUT_RENDER_STATUS, wxCommandEvent);

namespace gui::layoutstatus {

// Emits a command event carrying layout render progress text for UI feedback.
void PostLayoutRenderStatus(wxWindow *eventSource, wxWindow *targetWindow,
                            const wxString &statusText) {
  if (!eventSource || !targetWindow)
    return;
  wxCommandEvent event(EVT_LAYOUT_RENDER_STATUS);
  event.SetEventObject(eventSource);
  event.SetString(statusText);
  wxPostEvent(targetWindow, event);
}

} // namespace gui::layoutstatus
