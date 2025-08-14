/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
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

    grid->Add(new wxStaticText(this, wxID_ANY, "Fixture ID:"), 0, wxALIGN_CENTER_VERTICAL);
    idCtrl = new wxTextCtrl(this, wxID_ANY, "0");
    grid->Add(idCtrl, 1, wxEXPAND);

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
    SetSize(wxSize(450, GetSize().GetHeight()));
}

int AddFixtureDialog::GetUnitCount() const {
    return unitsCtrl->GetValue();
}

std::string AddFixtureDialog::GetFixtureName() const {
    return std::string(nameCtrl->GetValue().mb_str());
}

int AddFixtureDialog::GetFixtureId() const {
    long v = 0;
    idCtrl->GetValue().ToLong(&v);
    return static_cast<int>(v);
}

std::string AddFixtureDialog::GetMode() const {
    if (modeCtrl->GetCount() > 0)
        return std::string(modeCtrl->GetStringSelection().mb_str());
    return {};
}
