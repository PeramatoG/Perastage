#include "logindialog.h"

GdtfLoginDialog::GdtfLoginDialog(wxWindow* parent, const std::string& user, const std::string& pass)
    : wxDialog(parent, wxID_ANY, "GDTF Share Login", wxDefaultPosition, wxDefaultSize)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

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
