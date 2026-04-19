#pragma once

#include <wx/panel.h>

class wxButton;
class wxTextCtrl;

class GdtfCredentialsPanel : public wxPanel {
public:
  explicit GdtfCredentialsPanel(wxWindow *parent);

  void LoadCredentials();
  bool ApplyCredentials();

private:
  void OnValidateCredentials(wxCommandEvent &event);

  wxTextCtrl *usernameCtrl = nullptr;
  wxTextCtrl *passwordCtrl = nullptr;
  wxButton *validateButton = nullptr;
};
