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
#include "exporttrussdialog.h"

ExportTrussDialog::ExportTrussDialog(wxWindow* parent, const std::vector<std::string>& names)
    : wxDialog(parent, wxID_ANY, "Export Truss", wxDefaultPosition, wxDefaultSize)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    wxArrayString items;
    for (const auto& n : names)
        items.push_back(wxString::FromUTF8(n));
    listBox = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, items);
    if (listBox->GetCount() > 0)
        listBox->SetSelection(0);
    sizer->Add(listBox, 1, wxEXPAND | wxALL, 10);
    sizer->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 10);
    SetSizerAndFit(sizer);
}

std::string ExportTrussDialog::GetSelectedName() const
{
    if (listBox && listBox->GetSelection() != wxNOT_FOUND)
        return std::string(listBox->GetStringSelection().mb_str());
    return {};
}
