#pragma once

#include "update/app_update_service.h"

#include <wx/window.h>

namespace gui::update {

// Describes the user's choice from an available-update prompt.
struct UpdateNotificationChoice {
  bool openReleasePage = false;
  bool suppressVersionReminder = false;
};

// Shows an available-update prompt and optionally lets the user suppress this version.
UpdateNotificationChoice ShowAvailableUpdateDialog(wxWindow *parent,
                                                   const CheckResult &result,
                                                   bool allowVersionSuppression);

} // namespace gui::update
