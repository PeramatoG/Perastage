#include "preferencesdialog.h"
#include "configmanager.h"

#include <wx/notebook.h>

PreferencesDialog::PreferencesDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, "Preferences", wxDefaultPosition,
               wxDefaultSize) {
  wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);
  wxNotebook *book = new wxNotebook(this, wxID_ANY);

  // Rider Import page
  wxPanel *riderPanel = new wxPanel(book);
  wxBoxSizer *riderSizer = new wxBoxSizer(wxVERTICAL);
  wxFlexGridSizer *grid = new wxFlexGridSizer(2, 5, 5);
  grid->AddGrowableCol(1, 1);

  ConfigManager &cfg = ConfigManager::Get();
  for (int i = 0; i < 6; ++i) {
    wxString label = wxString::Format("LX%d height (m):", i + 1);
    grid->Add(new wxStaticText(riderPanel, wxID_ANY, label), 0,
              wxALIGN_CENTER_VERTICAL);
    double val = cfg.GetFloat("rider_lx" + std::to_string(i + 1) + "_height");
    lxCtrls[i] =
        new wxTextCtrl(riderPanel, wxID_ANY, wxString::Format("%.2f", val));
    grid->Add(lxCtrls[i], 1, wxEXPAND);
  }
  riderSizer->Add(grid, 1, wxALL | wxEXPAND, 10);
  riderPanel->SetSizer(riderSizer);

  book->AddPage(riderPanel, "Rider Import");

  topSizer->Add(book, 1, wxEXPAND | wxALL, 5);
  topSizer->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0,
                wxALL | wxEXPAND, 5);

  SetSizerAndFit(topSizer);

  Bind(wxEVT_BUTTON, [this](wxCommandEvent &evt) {
    if (evt.GetId() == wxID_OK) {
      ConfigManager &cfg = ConfigManager::Get();
      for (int i = 0; i < 6; ++i) {
        double v = 0.0;
        lxCtrls[i]->GetValue().ToDouble(&v);
        cfg.SetFloat("rider_lx" + std::to_string(i + 1) + "_height",
                     static_cast<float>(v));
      }
    }
    evt.Skip();
  });
}
