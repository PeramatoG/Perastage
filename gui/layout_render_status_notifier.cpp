#include "layout_render_status_notifier.h"

#include <wx/window.h>

wxDEFINE_EVENT(EVT_LAYOUT_RENDER_STATUS, wxCommandEvent);

namespace gui::layoutstatus {

// Dispatches layout render progress text immediately so the status bar updates during long tasks.
void PostLayoutRenderStatus(wxWindow *eventSource, wxWindow *targetWindow,
                            const wxString &statusText) {
  if (!eventSource || !targetWindow)
    return;
  wxCommandEvent event(EVT_LAYOUT_RENDER_STATUS);
  event.SetEventObject(eventSource);
  event.SetString(statusText);
  targetWindow->GetEventHandler()->ProcessEvent(event);
  targetWindow->Update();
}

} // namespace gui::layoutstatus
