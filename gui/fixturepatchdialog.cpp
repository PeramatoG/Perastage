#include "fixturepatchdialog.h"
#include <wx/tokenzr.h>

FixturePatchDialog::FixturePatchDialog(wxWindow* parent, const Fixture& fixture)
    : wxDialog(parent, wxID_ANY,
        wxString::Format("Edit fixture %s", wxString::FromUTF8(fixture.name)),
        wxDefaultPosition, wxDefaultSize)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    wxFlexGridSizer* grid = new wxFlexGridSizer(2, 5, 5);
    grid->Add(new wxStaticText(this, wxID_ANY, "Fixture ID:"), 0, wxALIGN_CENTER_VERTICAL);
    idCtrl = new wxTextCtrl(this, wxID_ANY, wxString::Format("%d", fixture.fixtureId));
    grid->Add(idCtrl, 1, wxEXPAND);

    long uni = 0, ch = 0;
    if (!fixture.address.empty()) {
        wxStringTokenizer tk(wxString::FromUTF8(fixture.address), ".");
        if (tk.HasMoreTokens()) tk.GetNextToken().ToLong(&uni);
        if (tk.HasMoreTokens()) tk.GetNextToken().ToLong(&ch);
    }
    grid->Add(new wxStaticText(this, wxID_ANY, "Universe:"), 0, wxALIGN_CENTER_VERTICAL);
    uniCtrl = new wxTextCtrl(this, wxID_ANY, wxString::Format("%ld", uni));
    grid->Add(uniCtrl, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, "Channel:"), 0, wxALIGN_CENTER_VERTICAL);
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
