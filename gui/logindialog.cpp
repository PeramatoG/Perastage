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
#include "logindialog.h"

GdtfLoginDialog::GdtfLoginDialog(wxWindow* parent, const std::string& user, const std::string& pass)
    : wxDialog(parent, wxID_ANY, "GDTF Share Login", wxDefaultPosition, wxDefaultSize)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* headerSizer = new wxBoxSizer(wxHORIZONTAL);
    headerSizer->AddStretchSpacer(1);
    wxButton* helpButton = new wxButton(this, wxID_ANY, "?", wxDefaultPosition, wxSize(22, 22), wxBU_EXACTFIT);
    helpButton->SetToolTip("You must be registered at https://gdtf-share.com/ to download GDTF files.");
    headerSizer->Add(helpButton, 0);
    sizer->Add(headerSizer, 0, wxLEFT | wxRIGHT | wxTOP | wxEXPAND, 10);

    wxFlexGridSizer* grid = new wxFlexGridSizer(2, 5, 5);
    grid->Add(new wxStaticText(this, wxID_ANY, "Username:"), 0, wxALIGN_CENTER_VERTICAL);
    userCtrl = new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(user),
                              wxDefaultPosition, wxSize(250, -1));
    grid->Add(userCtrl, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, "Password:"), 0, wxALIGN_CENTER_VERTICAL);
    passCtrl = new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(pass),
                              wxDefaultPosition, wxSize(250, -1), wxTE_PASSWORD);
    grid->Add(passCtrl, 1, wxEXPAND);

    grid->AddGrowableCol(1, 1);
    sizer->Add(grid, 0, wxALL | wxEXPAND, 10);

    sizer->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0, wxALL | wxEXPAND, 10);

    SetSizerAndFit(sizer);
}

std::string GdtfLoginDialog::GetUsername() const {
    return std::string(userCtrl->GetValue().mb_str());
}

std::string GdtfLoginDialog::GetPassword() const {
    return std::string(passCtrl->GetValue().mb_str());
}
