#include "preferences/mvr_preferences_page.h"
#include "guiconfigservices.h"
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/radiobut.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/statline.h>
#include <wx/textctrl.h>
#include "mvr_preferences.h"

MvrPreferencesPage::MvrPreferencesPage(wxWindow *parent)
    : PreferencesPage(parent) {
  wxBoxSizer *mvrSizer = new wxBoxSizer(wxVERTICAL);
  wxStaticBoxSizer *mvrExportSizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Export"));
  wxFlexGridSizer *mvrExportGrid = new wxFlexGridSizer(1, 2, 10, 10);
  mvrExportGrid->AddGrowableCol(1, 1);
  mvrExportGrid->Add(new wxStaticText(mvrExportSizer->GetStaticBox(), wxID_ANY,
                                      _("Truss geometry export mode:")),
                     0, wxALIGN_CENTER_VERTICAL);
  mvrTrussGeometryExportModeChoice =
      new wxChoice(mvrExportSizer->GetStaticBox(), wxID_ANY);
  mvrTrussGeometryExportModeChoice->Append(_("Standard MVR representation"));
  mvrTrussGeometryExportModeChoice->Append(
      _("Direct Geometry3D for truss symbols"));
  mvrExportGrid->Add(mvrTrussGeometryExportModeChoice, 1, wxEXPAND);
  mvrExportSizer->Add(mvrExportGrid, 0, wxALL | wxEXPAND, 8);
  wxStaticText *mvrExportHint =
      new wxStaticText(mvrExportSizer->GetStaticBox(), wxID_ANY,
                       _("Standard MVR representation: preserves imported "
                         "Symbol/Symdef references when possible and is "
                         "Perastage's canonical representation.\n"
                         "Direct Geometry3D for truss symbols: expands truss "
                         "Symbol/Symdef references into direct Geometry3D "
                         "entries for compatibility with applications that "
                         "do not correctly support Symbol/Symdef. Both modes "
                         "are MVR 1.6 compliant."));
  mvrExportHint->SetForegroundColour(
      wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
  mvrExportSizer->Add(mvrExportHint, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);
  mvrSizer->Add(mvrExportSizer, 0, wxALL | wxEXPAND, 10);
  SetSizer(mvrSizer);

  PrepareLayout();
}

void MvrPreferencesPage::LoadPreferences(const IGuiPreferencesService &cfg) {
  const MvrExportOptions mvrExportOptions =
      mvr::preferences::LoadExportOptions(cfg);
  mvrTrussGeometryExportModeChoice->SetSelection(
      mvrExportOptions.trussGeometryExportMode ==
              MvrTrussGeometryExportMode::DirectGeometry3DForTrussSymbols
          ? 1
          : 0);
}

void MvrPreferencesPage::ApplyPreferences(IGuiPreferencesService &cfg) {
  MvrExportOptions mvrExportOptions;
  mvrExportOptions.trussGeometryExportMode =
      mvrTrussGeometryExportModeChoice &&
              mvrTrussGeometryExportModeChoice->GetSelection() == 1
          ? MvrTrussGeometryExportMode::DirectGeometry3DForTrussSymbols
          : MvrTrussGeometryExportMode::Standard;
  mvr::preferences::SaveExportOptions(cfg, mvrExportOptions);

}
