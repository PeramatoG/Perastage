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
#include "addressdialog.h"

AddressDialog::AddressDialog(wxWindow* parent, int universe, int channel)
    : wxDialog(parent, wxID_ANY, "Edit Address", wxDefaultPosition, wxDefaultSize)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    wxFlexGridSizer* grid = new wxFlexGridSizer(2, 5, 5);
    grid->Add(new wxStaticText(this, wxID_ANY, "Universe:"), 0, wxALIGN_CENTER_VERTICAL);
    uniCtrl = new wxTextCtrl(this, wxID_ANY, wxString::Format("%d", universe));
    grid->Add(uniCtrl, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, "Channel:"), 0, wxALIGN_CENTER_VERTICAL);
    chCtrl = new wxTextCtrl(this, wxID_ANY, wxString::Format("%d", channel));
    grid->Add(chCtrl, 1, wxEXPAND);

    grid->AddGrowableCol(1, 1);
    sizer->Add(grid, 0, wxALL | wxEXPAND, 10);
    sizer->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0, wxALL | wxEXPAND, 10);

    SetSizerAndFit(sizer);
}

int AddressDialog::GetUniverse() const {
    long v = 0; uniCtrl->GetValue().ToLong(&v); return static_cast<int>(v);
}

int AddressDialog::GetChannel() const {
    long v = 0; chCtrl->GetValue().ToLong(&v); return static_cast<int>(v);
}
