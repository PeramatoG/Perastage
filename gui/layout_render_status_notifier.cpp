#include "layout_render_status_notifier.h"

#include <chrono>
#include <wx/window.h>

wxDEFINE_EVENT(EVT_LAYOUT_RENDER_STATUS, wxCommandEvent);

namespace gui::layoutstatus {

// Dispatches throttled layout render progress text to avoid UI update overhead in tight render loops.
void PostLayoutRenderStatus(wxWindow *eventSource, wxWindow *targetWindow,
                            const wxString &statusText) {
  if (!eventSource || !targetWindow)
    return;
  static wxString lastStatusText;
  static auto lastDispatch = std::chrono::steady_clock::time_point{};
  const auto now = std::chrono::steady_clock::now();
  constexpr auto kDispatchInterval = std::chrono::milliseconds(120);
  if (statusText == lastStatusText &&
      now - lastDispatch < kDispatchInterval) {
    return;
  }
  wxCommandEvent event(EVT_LAYOUT_RENDER_STATUS);
  event.SetEventObject(eventSource);
  event.SetString(statusText);
  wxPostEvent(targetWindow, event);
  lastStatusText = statusText;
  lastDispatch = now;
}

} // namespace gui::layoutstatus
