#include "inspection/inspector_navigation_dispatch.h"

#include <wx/event.h>

namespace gui::inspection {

// Queues copied nested navigation until the current GUI dispatch has returned.
void QueueNestedGdtfNavigation(wxEvtHandler &owner,
                               NestedGdtfNavigationRequest request,
                               NestedGdtfNavigationCallback callback) {
  owner.CallAfter(
      [request = std::move(request), callback = std::move(callback)]() mutable {
        callback(std::move(request));
      });
}

} // namespace gui::inspection
