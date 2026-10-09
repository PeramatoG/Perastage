#pragma once

#include <wx/panel.h>

class IGuiPreferencesService;
class wxChoice;

class GdtfMutationPolicyPanel : public wxPanel {
public:
  explicit GdtfMutationPolicyPanel(wxWindow *parent);

  void LoadPreferences(const IGuiPreferencesService &preferences);
  void ApplyPreferences(IGuiPreferencesService &preferences) const;

private:
  wxChoice *policyChoice = nullptr;
};
