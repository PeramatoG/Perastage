#include "update/update_notification_dialog.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/event.h>
#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/string.h>

namespace gui::update {
namespace {

// Builds the message shown when a newer application version is available.
wxString BuildAvailableUpdateMessage(const CheckResult &result) {
  return "A newer Perastage version is available.\nCurrent version: " +
         wxString::FromUTF8(result.currentVersion) +
         "\nLatest version: " + wxString::FromUTF8(result.latestVersion) +
         "\n\nOpen release page now?";
}

} // namespace

// Shows an available-update prompt and optionally lets the user suppress this version.
UpdateNotificationChoice ShowAvailableUpdateDialog(
    wxWindow *parent, const CheckResult &result,
    const bool allowVersionSuppression) {
  UpdateNotificationChoice choice;

  wxDialog dialog(parent, wxID_ANY, _("Perastage Updates"), wxDefaultPosition,
                  wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
  wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);

  wxStaticText *message = new wxStaticText(&dialog, wxID_ANY,
                                           BuildAvailableUpdateMessage(result));
  message->Wrap(420);
  topSizer->Add(message, 0, wxALL | wxEXPAND, 12);

  wxCheckBox *suppressReminderCheck = nullptr;
  if (allowVersionSuppression) {
    suppressReminderCheck = new wxCheckBox(
        &dialog, wxID_ANY,
        _("Do not remind me again for this version"));
    topSizer->Add(suppressReminderCheck, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);
  }

  wxBoxSizer *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
  buttonSizer->AddStretchSpacer(1);
  wxButton *yesButton = new wxButton(&dialog, wxID_YES, _("Yes"));
  wxButton *noButton = new wxButton(&dialog, wxID_NO, _("No"));
  yesButton->Bind(wxEVT_BUTTON, [&dialog](wxCommandEvent &) {
    dialog.EndModal(wxID_YES);
  });
  noButton->Bind(wxEVT_BUTTON, [&dialog](wxCommandEvent &) {
    dialog.EndModal(wxID_NO);
  });
  buttonSizer->Add(yesButton, 0, wxRIGHT, 8);
  buttonSizer->Add(noButton, 0);
  topSizer->Add(buttonSizer, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 12);

  dialog.SetSizerAndFit(topSizer);
  dialog.CentreOnParent();
  yesButton->SetDefault();

  const int response = dialog.ShowModal();
  choice.openReleasePage = response == wxID_YES;
  choice.suppressVersionReminder =
      suppressReminderCheck && suppressReminderCheck->GetValue();
  return choice;
}

} // namespace gui::update
