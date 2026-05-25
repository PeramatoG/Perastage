#include "layout_render_status_notifier.h"

#include <chrono>
#include <wx/app.h>
#include <wx/window.h>

wxDEFINE_EVENT(EVT_LAYOUT_RENDER_STATUS, wxCommandEvent);

namespace gui::layoutstatus {

// Processes queued UI work sparingly so status messages can refresh during heavy layout rebuild loops.
void PumpLayoutRenderStatusUi() {
  static auto lastPump = std::chrono::steady_clock::time_point{};
  static bool isPumping = false;
  const auto now = std::chrono::steady_clock::now();
  constexpr auto kPumpInterval = std::chrono::milliseconds(80);
  if (isPumping || now - lastPump < kPumpInterval)
    return;
  if (!wxTheApp)
    return;

  isPumping = true;
  wxTheApp->ProcessPendingEvents();
  isPumping = false;
  lastPump = now;
}

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
  PumpLayoutRenderStatusUi();
  lastStatusText = statusText;
  lastDispatch = now;
}

} // namespace gui::layoutstatus
