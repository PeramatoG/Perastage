#pragma once
#include <wx/panel.h>
class IGuiPreferencesService;
class wxChoice;
class wxCheckBox;
class wxRadioButton;

class Viewer3DRenderingPreferencesPanel : public wxPanel {
public:
  explicit Viewer3DRenderingPreferencesPanel(wxWindow *parent);
  void LoadPreferences(const IGuiPreferencesService &preferences);
  void ApplyPreferences(IGuiPreferencesService &preferences) const;
private:
  wxChoice *detailChoice = nullptr;
  wxCheckBox *movingProxyCheck = nullptr;
  wxRadioButton *viewer3dStandardRenderRadio = nullptr;
  wxRadioButton *viewer3dWhiteRenderRadio = nullptr;
  wxRadioButton *viewer3dWhiteModelRenderRadio = nullptr;
  wxRadioButton *viewer3dTexturedRenderRadio = nullptr;
  wxRadioButton *viewer3dWireframeRenderRadio = nullptr;
  wxRadioButton *viewer3dByDeviceTypeRenderRadio = nullptr;
  wxRadioButton *viewer3dByLayerRenderRadio = nullptr;
  wxRadioButton *viewer3dByUniverseRenderRadio = nullptr;
};
