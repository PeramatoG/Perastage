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

    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* openBtn = new wxButton(this, wxID_OPEN, "Add from file...");
    openBtn->Bind(wxEVT_BUTTON, &SelectFixtureTypeDialog::OnOpen, this);
    btnSizer->Add(openBtn, 0, wxRIGHT, 5);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(new wxButton(this, wxID_OK), 0, wxRIGHT, 5);
    btnSizer->Add(new wxButton(this, wxID_CANCEL), 0);
    sizer->Add(btnSizer, 0, wxEXPAND | wxALL, 5);

    SetSizerAndFit(sizer);
    SetSize(wxSize(400, GetSize().GetHeight()));
}

int SelectFixtureTypeDialog::GetSelection() const
{
    return listCtrl->GetSelection();
}

void SelectFixtureTypeDialog::OnOpen(wxCommandEvent&)
{
    EndModal(wxID_OPEN);
}
