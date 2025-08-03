#include "selectfixturetypedialog.h"

SelectFixtureTypeDialog::SelectFixtureTypeDialog(wxWindow* parent, const std::vector<std::string>& types)
    : wxDialog(parent, wxID_ANY, "Select Fixture Type", wxDefaultPosition, wxDefaultSize)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(new wxStaticText(this, wxID_ANY, "Choose a fixture type:"), 0, wxALL, 5);

    wxArrayString items;
    for (const auto& t : types)
        items.push_back(wxString::FromUTF8(t));
    listCtrl = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, items);
    if (listCtrl->GetCount() > 0)
        listCtrl->SetSelection(0);
    sizer->Add(listCtrl, 1, wxALL | wxEXPAND, 5);

    wxStdDialogButtonSizer* btnSizer = new wxStdDialogButtonSizer();
    btnSizer->AddButton(new wxButton(this, wxID_OPEN, "Add from file..."));
    btnSizer->AddButton(new wxButton(this, wxID_OK));
    btnSizer->AddButton(new wxButton(this, wxID_CANCEL));
    btnSizer->Realize();
    sizer->Add(btnSizer, 0, wxALIGN_RIGHT | wxALL, 5);

    SetSizerAndFit(sizer);
}

int SelectFixtureTypeDialog::GetSelection() const
{
    return listCtrl->GetSelection();
}
