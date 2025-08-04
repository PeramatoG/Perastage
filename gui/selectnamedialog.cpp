#include "selectnamedialog.h"

SelectNameDialog::SelectNameDialog(wxWindow* parent,
                                   const std::vector<std::string>& names,
                                   const wxString& title,
                                   const wxString& message)
    : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(new wxStaticText(this, wxID_ANY, message), 0, wxALL, 5);

    wxArrayString items;
    for (const auto& n : names)
        items.push_back(wxString::FromUTF8(n));
    listCtrl = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, items);
    if (listCtrl->GetCount() > 0)
        listCtrl->SetSelection(0);
    sizer->Add(listCtrl, 1, wxALL | wxEXPAND, 5);

    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* openBtn = new wxButton(this, wxID_OPEN, "Add from file...");
    openBtn->Bind(wxEVT_BUTTON, &SelectNameDialog::OnOpen, this);
    btnSizer->Add(openBtn, 0, wxRIGHT, 5);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(new wxButton(this, wxID_OK), 0, wxRIGHT, 5);
    btnSizer->Add(new wxButton(this, wxID_CANCEL), 0);
    sizer->Add(btnSizer, 0, wxEXPAND | wxALL, 5);

    SetSizerAndFit(sizer);
    SetSize(wxSize(400, GetSize().GetHeight()));
}

int SelectNameDialog::GetSelection() const
{
    return listCtrl->GetSelection();
}

void SelectNameDialog::OnOpen(wxCommandEvent&)
{
    EndModal(wxID_OPEN);
}
