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
