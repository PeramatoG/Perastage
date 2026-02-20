#include "dialogs/generatefixturesymbolsdialog.h"

#include <wx/button.h>
#include <wx/listbox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

GenerateFixtureSymbolsDialog::GenerateFixtureSymbolsDialog(
    wxWindow *parent, const std::vector<FixtureTypeOption> &options)
    : wxDialog(parent, wxID_ANY, "Generate Fixture Symbols",
               wxDefaultPosition, wxSize(480, 360),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
  auto *root = new wxBoxSizer(wxVERTICAL);
  root->Add(new wxStaticText(this, wxID_ANY,
                             "Select one fixture type from the current project:"),
            0, wxALL, 8);

  listBox = new wxListBox(this, wxID_ANY);
  for (const auto &option : options) {
    const wxString line = wxString::Format("%s (%d instance%s)",
                                           wxString::FromUTF8(option.typeName),
                                           option.instanceCount,
                                           option.instanceCount == 1 ? "" : "s");
    listBox->Append(line);
  }
  if (!options.empty())
    listBox->SetSelection(0);

  root->Add(listBox, 1, wxEXPAND | wxALL, 8);

  auto *buttons = new wxStdDialogButtonSizer();
  auto *ok = new wxButton(this, wxID_OK, "Generate");
  auto *cancel = new wxButton(this, wxID_CANCEL, "Cancel");
  buttons->AddButton(ok);
  buttons->AddButton(cancel);
  buttons->Realize();
  root->Add(buttons, 0, wxEXPAND | wxALL, 8);

  ok->Bind(wxEVT_BUTTON, &GenerateFixtureSymbolsDialog::OnGenerate, this);

  SetSizerAndFit(root);
}

int GenerateFixtureSymbolsDialog::GetSelectedIndex() const {
  return listBox ? listBox->GetSelection() : wxNOT_FOUND;
}

void GenerateFixtureSymbolsDialog::OnGenerate(wxCommandEvent &event) {
  if (!listBox || listBox->GetSelection() == wxNOT_FOUND)
    return;
  EndModal(wxID_OK);
  event.Skip(false);
}
