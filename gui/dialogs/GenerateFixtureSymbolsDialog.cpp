#include "dialogs/GenerateFixtureSymbolsDialog.h"

#include <wx/listbox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

GenerateFixtureSymbolsDialog::GenerateFixtureSymbolsDialog(
    wxWindow *parent, const std::vector<FixtureSymbolTypeOption> &options)
    : wxDialog(parent, wxID_ANY, "Generate Fixture Symbols") {
  auto *root = new wxBoxSizer(wxVERTICAL);
  root->Add(new wxStaticText(this, wxID_ANY,
                             "Select fixture type to generate symbols:"),
            0, wxALL, 8);

  wxArrayString items;
  for (const auto &option : options)
    items.push_back(wxString::FromUTF8(option.label));

  list_ = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(460, 240), items);
  if (!items.empty())
    list_->SetSelection(0);
  root->Add(list_, 1, wxEXPAND | wxLEFT | wxRIGHT, 8);

  root->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0,
            wxEXPAND | wxALL, 8);

  SetSizerAndFit(root);
}

int GenerateFixtureSymbolsDialog::GetSelectionIndex() const {
  return list_ ? list_->GetSelection() : wxNOT_FOUND;
}
