#pragma once

#include "inspection/inspector_navigation_request.h"

#include <functional>

class wxEvtHandler;

namespace gui::inspection {

using NestedGdtfNavigationCallback =
    std::function<void(NestedGdtfNavigationRequest)>;

// Queues copied nested navigation until the current GUI dispatch has returned.
void QueueNestedGdtfNavigation(wxEvtHandler &owner,
                               NestedGdtfNavigationRequest request,
                               NestedGdtfNavigationCallback callback);

} // namespace gui::inspection
