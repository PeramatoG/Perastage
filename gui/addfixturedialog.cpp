#include "addfixturedialog.h"

AddFixtureDialog::AddFixtureDialog(wxWindow* parent,
                                   const wxString& defaultName,
                                   const std::vector<std::string>& modes)
    : wxDialog(parent, wxID_ANY, "Add Fixture", wxDefaultPosition, wxDefaultSize)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    wxFlexGridSizer* grid = new wxFlexGridSizer(2, 5, 5);
    grid->Add(new wxStaticText(this, wxID_ANY, "Units:"), 0, wxALIGN_CENTER_VERTICAL);
    unitsCtrl = new wxSpinCtrl(this, wxID_ANY);
    unitsCtrl->SetRange(1, 9999);
    unitsCtrl->SetValue(1);
    grid->Add(unitsCtrl, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, "Name:"), 0, wxALIGN_CENTER_VERTICAL);
    nameCtrl = new wxTextCtrl(this, wxID_ANY, defaultName);
    grid->Add(nameCtrl, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, "Mode:"), 0, wxALIGN_CENTER_VERTICAL);
    wxArrayString choices;
    for (const auto& m : modes)
        choices.push_back(wxString::FromUTF8(m));
    modeCtrl = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, choices);
    if (modeCtrl->GetCount() > 0)
        modeCtrl->SetSelection(0);
    grid->Add(modeCtrl, 1, wxEXPAND);

    grid->AddGrowableCol(1, 1);
    sizer->Add(grid, 0, wxALL | wxEXPAND, 10);
    sizer->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0, wxALL | wxEXPAND, 10);

    SetSizerAndFit(sizer);
}

int AddFixtureDialog::GetUnitCount() const {
    return unitsCtrl->GetValue();
}

std::string AddFixtureDialog::GetFixtureName() const {
    return std::string(nameCtrl->GetValue().mb_str());
}

std::string AddFixtureDialog::GetMode() const {
    if (modeCtrl->GetCount() > 0)
        return std::string(modeCtrl->GetStringSelection().mb_str());
    return {};
}
