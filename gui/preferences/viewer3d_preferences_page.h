#pragma once

#include "preferences/preferences_page.h"

class wxCheckBox;
class Viewer3DRenderingPreferencesPanel;

class Viewer3DPreferencesPage : public PreferencesPage {
public:
  explicit Viewer3DPreferencesPage(wxWindow *parent);
  void LoadPreferences(const IGuiPreferencesService &preferences) override;
  void ApplyPreferences(IGuiPreferencesService &preferences) override;

private:
  Viewer3DRenderingPreferencesPanel *viewer3dRenderingPanel = nullptr;
  wxCheckBox *viewer3dInvertOrbitHorizontalCheck = nullptr;
  wxCheckBox *viewer3dInvertOrbitVerticalCheck = nullptr;
};
