#include "preferences/viewer3d_rendering_preferences_panel.h"
#include "guiconfigservices.h"
#include "model_detail_policy.h"
#include "viewer3d_render_style.h"
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/radiobut.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/statbox.h>

Viewer3DRenderingPreferencesPanel::Viewer3DRenderingPreferencesPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY) {
  auto *sizer = new wxBoxSizer(wxVERTICAL);
  auto *performance = new wxStaticBoxSizer(wxVERTICAL, this, _("Performance"));
  auto *row = new wxBoxSizer(wxHORIZONTAL);
  row->Add(new wxStaticText(performance->GetStaticBox(), wxID_ANY, _("Model detail:")),
           0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
  detailChoice = new wxChoice(performance->GetStaticBox(), wxID_ANY);
  detailChoice->Append(_("Low"));
  detailChoice->Append(_("Standard"));
  detailChoice->Append(_("High"));
  row->Add(detailChoice, 1, wxEXPAND);
  performance->Add(row, 0, wxALL | wxEXPAND, 8);
  movingProxyCheck = new wxCheckBox(performance->GetStaticBox(), wxID_ANY,
      _("Use a simplified proxy while navigating"));
  performance->Add(movingProxyCheck, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);
  auto *hint = new wxStaticText(performance->GetStaticBox(), wxID_ANY,
      _("Standard balances detail and performance. High preserves full model detail. "
        "Low simplifies heavy meshes more aggressively. GDTF resources stay unchanged."));
  hint->Wrap(740);
  performance->Add(hint, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);
  sizer->Add(performance, 0, wxEXPAND);
  wxStaticBoxSizer *viewer3dRenderSizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Render mode"));
  viewer3dStandardRenderRadio = new wxRadioButton(
      viewer3dRenderSizer->GetStaticBox(), wxID_ANY, _("Standard"),
      wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
  viewer3dWhiteRenderRadio = new wxRadioButton(
      viewer3dRenderSizer->GetStaticBox(), wxID_ANY, _("White"));
  viewer3dWhiteModelRenderRadio = new wxRadioButton(
      viewer3dRenderSizer->GetStaticBox(), wxID_ANY, _("Sketch mode"));
  viewer3dTexturedRenderRadio = new wxRadioButton(
      viewer3dRenderSizer->GetStaticBox(), wxID_ANY, _("Textured"));
  viewer3dWireframeRenderRadio = new wxRadioButton(
      viewer3dRenderSizer->GetStaticBox(), wxID_ANY, _("Wireframe"));
  viewer3dByDeviceTypeRenderRadio = new wxRadioButton(
      viewer3dRenderSizer->GetStaticBox(), wxID_ANY, _("By device type"));
  viewer3dByLayerRenderRadio = new wxRadioButton(
      viewer3dRenderSizer->GetStaticBox(), wxID_ANY, _("By layer"));
  viewer3dByUniverseRenderRadio = new wxRadioButton(
      viewer3dRenderSizer->GetStaticBox(), wxID_ANY, _("By universe"));

  viewer3dRenderSizer->Add(viewer3dStandardRenderRadio, 0,
                           wxLEFT | wxRIGHT | wxTOP, 8);
  viewer3dRenderSizer->Add(viewer3dWhiteModelRenderRadio, 0,
                           wxLEFT | wxRIGHT | wxTOP, 6);
  viewer3dRenderSizer->Add(viewer3dTexturedRenderRadio, 0,
                           wxLEFT | wxRIGHT | wxTOP, 6);
  viewer3dRenderSizer->Add(viewer3dWireframeRenderRadio, 0,
                           wxLEFT | wxRIGHT | wxTOP, 6);
  viewer3dRenderSizer->Add(viewer3dWhiteRenderRadio, 0,
                           wxLEFT | wxRIGHT | wxTOP, 6);
  viewer3dRenderSizer->Add(viewer3dByDeviceTypeRenderRadio, 0,
                           wxLEFT | wxRIGHT | wxTOP, 6);
  viewer3dRenderSizer->Add(viewer3dByLayerRenderRadio, 0,
                           wxLEFT | wxRIGHT | wxTOP, 6);
  viewer3dRenderSizer->Add(viewer3dByUniverseRenderRadio, 0,
                           wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 8);
  sizer->Add(viewer3dRenderSizer, 0, wxEXPAND | wxTOP, 10);

  SetSizer(sizer);
}

void Viewer3DRenderingPreferencesPanel::LoadPreferences(
    const IGuiPreferencesService &preferences) {
  const auto detail = model_detail::ReadPreferences(preferences);
  detailChoice->SetSelection(static_cast<int>(detail.level));
  movingProxyCheck->SetValue(detail.movingProxy);
  const Viewer3DRenderStyle renderStyle = ParseViewer3DRenderStyle(
      preferences.GetValue("viewer3d_render_style"));
  viewer3dStandardRenderRadio->SetValue(renderStyle ==
                                        Viewer3DRenderStyle::Standard);
  viewer3dWhiteRenderRadio->SetValue(renderStyle == Viewer3DRenderStyle::White);
  viewer3dWhiteModelRenderRadio->SetValue(renderStyle ==
                                          Viewer3DRenderStyle::WhiteModel);
  viewer3dTexturedRenderRadio->SetValue(renderStyle ==
                                        Viewer3DRenderStyle::Textured);
  viewer3dWireframeRenderRadio->SetValue(renderStyle ==
                                         Viewer3DRenderStyle::Wireframe);
  viewer3dByDeviceTypeRenderRadio->SetValue(renderStyle ==
                                            Viewer3DRenderStyle::ByDeviceType);
  viewer3dByLayerRenderRadio->SetValue(renderStyle ==
                                       Viewer3DRenderStyle::ByLayer);
  viewer3dByUniverseRenderRadio->SetValue(renderStyle ==
                                          Viewer3DRenderStyle::ByUniverse);

}

void Viewer3DRenderingPreferencesPanel::ApplyPreferences(
    IGuiPreferencesService &preferences) const {
  const int index = detailChoice->GetSelection();
  model_detail::SavePreferences(preferences,
      {index >= 0 && index <= 2 ? static_cast<model_detail::Level>(index)
                              : model_detail::Level::Standard,
       movingProxyCheck->GetValue()});
  Viewer3DRenderStyle renderStyle = Viewer3DRenderStyle::Standard;
  if (viewer3dWhiteRenderRadio && viewer3dWhiteRenderRadio->GetValue())
    renderStyle = Viewer3DRenderStyle::White;
  else if (viewer3dWhiteModelRenderRadio &&
           viewer3dWhiteModelRenderRadio->GetValue())
    renderStyle = Viewer3DRenderStyle::WhiteModel;
  else if (viewer3dTexturedRenderRadio &&
           viewer3dTexturedRenderRadio->GetValue())
    renderStyle = Viewer3DRenderStyle::Textured;
  else if (viewer3dWireframeRenderRadio &&
           viewer3dWireframeRenderRadio->GetValue())
    renderStyle = Viewer3DRenderStyle::Wireframe;
  else if (viewer3dByDeviceTypeRenderRadio &&
           viewer3dByDeviceTypeRenderRadio->GetValue())
    renderStyle = Viewer3DRenderStyle::ByDeviceType;
  else if (viewer3dByLayerRenderRadio && viewer3dByLayerRenderRadio->GetValue())
    renderStyle = Viewer3DRenderStyle::ByLayer;
  else if (viewer3dByUniverseRenderRadio &&
           viewer3dByUniverseRenderRadio->GetValue())
    renderStyle = Viewer3DRenderStyle::ByUniverse;
  preferences.SetValue("viewer3d_render_style", ToConfigValue(renderStyle));
}
