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
#include "fixturepatchdialog.h"
#include <wx/tokenzr.h>
#include <limits>

FixturePatchDialog::FixturePatchDialog(wxWindow* parent, const Fixture& fixture)
    : wxDialog(parent, wxID_ANY,
        wxString::FromUTF8(fixture.instanceName),
        wxDefaultPosition, wxDefaultSize)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    wxFlexGridSizer* grid = new wxFlexGridSizer(2, 5, 5);
    grid->Add(new wxStaticText(this, wxID_ANY, _("Fixture ID:")), 0, wxALIGN_CENTER_VERTICAL);
    idCtrl = new wxTextCtrl(this, wxID_ANY, wxString::Format("%d", fixture.fixtureId));
    grid->Add(idCtrl, 1, wxEXPAND);

    long uni = 0, ch = 0;
    if (!fixture.address.empty()) {
        wxStringTokenizer tk(wxString::FromUTF8(fixture.address), ".");
        if (tk.HasMoreTokens()) tk.GetNextToken().ToLong(&uni);
        if (tk.HasMoreTokens()) tk.GetNextToken().ToLong(&ch);
    }
    grid->Add(new wxStaticText(this, wxID_ANY, _("Universe:")), 0, wxALIGN_CENTER_VERTICAL);
    uniCtrl = new wxTextCtrl(this, wxID_ANY, wxString::Format("%ld", uni));
    grid->Add(uniCtrl, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, _("Channel:")), 0, wxALIGN_CENTER_VERTICAL);
    chCtrl = new wxTextCtrl(this, wxID_ANY, wxString::Format("%ld", ch));
    grid->Add(chCtrl, 1, wxEXPAND);

    grid->AddGrowableCol(1, 1);
    sizer->Add(grid, 0, wxALL | wxEXPAND, 10);

    sizer->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0, wxALL | wxEXPAND, 10);

    SetSizerAndFit(sizer);
}

int FixturePatchDialog::GetFixtureId() const {
    long v = 0; idCtrl->GetValue().ToLong(&v); return static_cast<int>(v);
}
int FixturePatchDialog::GetUniverse() const {
    long v = 0; uniCtrl->GetValue().ToLong(&v); return static_cast<int>(v);
}
int FixturePatchDialog::GetChannel() const {
    long v = 0; chCtrl->GetValue().ToLong(&v); return static_cast<int>(v);
}

bool FixturePatchDialog::Validate() {
    long id = 0, universe = 0, channel = 0;
    if (!idCtrl->GetValue().ToLong(&id) || id < 0 ||
        id > std::numeric_limits<int>::max() ||
        !uniCtrl->GetValue().ToLong(&universe) || universe < 0 ||
        universe > std::numeric_limits<int>::max() ||
        !chCtrl->GetValue().ToLong(&channel) || channel < 0 || channel > 512 ||
        ((universe == 0) != (channel == 0))) {
        wxMessageBox(_("Enter a non-negative fixture ID and a positive universe with channel 1–512, or 0/0 to unpatch."),
                     _("Edit DMX Patch"), wxOK | wxICON_ERROR, this);
        return false;
    }
    return wxDialog::Validate();
}
