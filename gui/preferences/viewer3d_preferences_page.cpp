#include "preferences/viewer3d_preferences_page.h"
#include "guiconfigservices.h"
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/radiobut.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/statline.h>
#include <wx/textctrl.h>
#include "user_navigation_preferences.h"
#include "preferences/viewer3d_rendering_preferences_panel.h"

Viewer3DPreferencesPage::Viewer3DPreferencesPage(wxWindow *parent)
    : PreferencesPage(parent) {
  SetBackgroundColour(
      wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
  wxBoxSizer *viewer3dSizer = new wxBoxSizer(wxVERTICAL);
  wxStaticText *viewer3dTitle =
      new wxStaticText(this, wxID_ANY, _("3D Viewer"));
  wxFont viewer3dTitleFont = viewer3dTitle->GetFont();
  viewer3dTitleFont.MakeBold();
  viewer3dTitleFont.SetPointSize(viewer3dTitleFont.GetPointSize() + 2);
  viewer3dTitle->SetFont(viewer3dTitleFont);
  viewer3dSizer->Add(viewer3dTitle, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 12);

  wxStaticText *viewer3dSubtitle =
      new wxStaticText(this, wxID_ANY,
                       _("Customize camera interaction and visualize the "
                         "current navigation shortcuts."));
  viewer3dSubtitle->SetForegroundColour(
      wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
  viewer3dSizer->Add(viewer3dSubtitle, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
                     6);
  viewer3dSizer->Add(new wxStaticLine(this, wxID_ANY), 0,
                     wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);

  wxStaticBoxSizer *viewer3dNavigationSizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Navigation"));
  viewer3dInvertOrbitHorizontalCheck =
      new wxCheckBox(viewer3dNavigationSizer->GetStaticBox(), wxID_ANY,
                     _("Invert orbit horizontal direction"));
  viewer3dNavigationSizer->Add(viewer3dInvertOrbitHorizontalCheck, 0,
                               wxLEFT | wxRIGHT | wxTOP, 8);

  viewer3dInvertOrbitVerticalCheck =
      new wxCheckBox(viewer3dNavigationSizer->GetStaticBox(), wxID_ANY,
                     _("Invert orbit vertical direction"));
  viewer3dNavigationSizer->Add(viewer3dInvertOrbitVerticalCheck, 0, wxALL, 8);

  wxStaticText *viewer3dNavigationHint = new wxStaticText(
      viewer3dNavigationSizer->GetStaticBox(), wxID_ANY,
      _("Disabled by default to preserve the current behavior."));
  viewer3dNavigationHint->SetForegroundColour(
      wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
  viewer3dNavigationSizer->Add(viewer3dNavigationHint, 0,
                               wxLEFT | wxRIGHT | wxBOTTOM, 8);
  viewer3dSizer->Add(viewer3dNavigationSizer, 0,
                     wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);

  viewer3dRenderingPanel = new Viewer3DRenderingPreferencesPanel(this);
  viewer3dSizer->Add(viewer3dRenderingPanel, 0,
                     wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);

  wxStaticBoxSizer *viewer3dShortcutsSizer = new wxStaticBoxSizer(
      wxVERTICAL, this, _("Current mouse and keyboard shortcuts"));
  wxStaticText *viewer3dShortcutsInfo = new wxStaticText(
      viewer3dShortcutsSizer->GetStaticBox(), wxID_ANY,
      _("Orbit: Left drag or Right drag.\n"
        "Pan: Middle drag, Shift + drag, or Shift + Left drag.\n"
        "Zoom: Mouse wheel.\n"
        "Selection rectangle: Ctrl + Left drag."));
  viewer3dShortcutsSizer->Add(viewer3dShortcutsInfo, 0, wxALL, 8);

  wxStaticText *viewer3dShortcutsHint = new wxStaticText(
      viewer3dShortcutsSizer->GetStaticBox(), wxID_ANY,
      _("Informational only: shortcut remapping is not available yet."));
  viewer3dShortcutsHint->SetForegroundColour(
      wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
  viewer3dShortcutsSizer->Add(viewer3dShortcutsHint, 0,
                              wxLEFT | wxRIGHT | wxBOTTOM, 8);
  viewer3dSizer->Add(viewer3dShortcutsSizer, 0,
                     wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 10);
  SetSizer(viewer3dSizer);

  PrepareLayout();
}

void Viewer3DPreferencesPage::LoadPreferences(const IGuiPreferencesService &cfg) {
  const auto viewer3dInvertOrbitHorizontalValue =
      cfg.GetValue(std::string(
          user_navigation_preferences::kHorizontalOrbitInversionConfigKey));
  viewer3dInvertOrbitHorizontalCheck->SetValue(
      viewer3dInvertOrbitHorizontalValue &&
      *viewer3dInvertOrbitHorizontalValue == "1");
  const auto viewer3dInvertOrbitValue = cfg.GetValue(
      std::string(user_navigation_preferences::kVerticalOrbitInversionConfigKey));
  viewer3dInvertOrbitVerticalCheck->SetValue(viewer3dInvertOrbitValue &&
                                             *viewer3dInvertOrbitValue == "1");
  viewer3dRenderingPanel->LoadPreferences(
      cfg);
}

void Viewer3DPreferencesPage::ApplyPreferences(IGuiPreferencesService &cfg) {
  if (viewer3dRenderingPanel)
    viewer3dRenderingPanel->ApplyPreferences(cfg);
  cfg.SetValue(std::string(user_navigation_preferences::kVerticalOrbitInversionConfigKey),
               viewer3dInvertOrbitVerticalCheck &&
                       viewer3dInvertOrbitVerticalCheck->GetValue()
                   ? "1"
                   : "0");
  cfg.SetValue(std::string(
                   user_navigation_preferences::kHorizontalOrbitInversionConfigKey),
               viewer3dInvertOrbitHorizontalCheck &&
                       viewer3dInvertOrbitHorizontalCheck->GetValue()
                   ? "1"
                   : "0");
}
