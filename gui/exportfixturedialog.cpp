#include "exportfixturedialog.h"

ExportFixtureDialog::ExportFixtureDialog(wxWindow* parent, const std::vector<std::string>& types)
    : wxDialog(parent, wxID_ANY, "Export Fixture", wxDefaultPosition, wxDefaultSize)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    wxArrayString items;
    for (const auto& t : types)
        items.push_back(wxString::FromUTF8(t));
    listBox = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, items);
    if (listBox->GetCount() > 0)
        listBox->SetSelection(0);
    sizer->Add(listBox, 1, wxEXPAND | wxALL, 10);
    sizer->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 10);
    SetSizerAndFit(sizer);
}

std::string ExportFixtureDialog::GetSelectedType() const
{
    if (listBox && listBox->GetSelection() != wxNOT_FOUND)
        return std::string(listBox->GetStringSelection().mb_str());
    return {};
}
