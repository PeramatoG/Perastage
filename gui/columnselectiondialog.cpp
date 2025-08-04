#include "columnselectiondialog.h"
#include <wx/sizer.h>

ColumnSelectionDialog::ColumnSelectionDialog(wxWindow* parent,
                                             const std::vector<std::string>& columns,
                                             const std::vector<int>& selected)
    : wxDialog(parent, wxID_ANY, "Select Columns", wxDefaultPosition, wxDefaultSize)
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* listSizer = new wxBoxSizer(wxHORIZONTAL);

    list = new wxCheckListBox(this, wxID_ANY);

    std::vector<bool> marked(columns.size(), false);
    // First append preselected columns in the provided order
    for (int idx : selected) {
        if (idx < 0 || static_cast<size_t>(idx) >= columns.size())
            continue;
        list->Append(wxString::FromUTF8(columns[idx]));
        list->Check(list->GetCount() - 1, true);
        indices.push_back(idx);
        marked[idx] = true;
    }
    // Append remaining columns unchecked
    for (size_t i = 0; i < columns.size(); ++i) {
        if (marked[i])
            continue;
        list->Append(wxString::FromUTF8(columns[i]));
        list->Check(list->GetCount() - 1, selected.empty());
        indices.push_back(static_cast<int>(i));
    }

    listSizer->Add(list, 1, wxEXPAND);

    wxBoxSizer* btnSizer = new wxBoxSizer(wxVERTICAL);
    wxButton* upBtn = new wxButton(this, wxID_ANY, "Up");
    wxButton* downBtn = new wxButton(this, wxID_ANY, "Down");
    btnSizer->Add(upBtn, 0, wxEXPAND | wxBOTTOM, 5);
    btnSizer->Add(downBtn, 0, wxEXPAND);
    listSizer->Add(btnSizer, 0, wxLEFT, 5);

    mainSizer->Add(listSizer, 1, wxEXPAND | wxALL, 10);

    wxBoxSizer* selSizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton* selectAllBtn = new wxButton(this, wxID_ANY, "Select All");
    wxButton* deselectAllBtn = new wxButton(this, wxID_ANY, "Deselect All");
    selSizer->Add(selectAllBtn, 0, wxRIGHT, 5);
    selSizer->Add(deselectAllBtn, 0);
    mainSizer->Add(selSizer, 0, wxALIGN_LEFT | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    mainSizer->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    SetSizerAndFit(mainSizer);

    upBtn->Bind(wxEVT_BUTTON, &ColumnSelectionDialog::OnUp, this);
    downBtn->Bind(wxEVT_BUTTON, &ColumnSelectionDialog::OnDown, this);
    selectAllBtn->Bind(wxEVT_BUTTON, &ColumnSelectionDialog::OnSelectAll, this);
    deselectAllBtn->Bind(wxEVT_BUTTON, &ColumnSelectionDialog::OnDeselectAll, this);
}

void ColumnSelectionDialog::OnUp(wxCommandEvent&)
{
    int sel = list->GetSelection();
    if (sel == wxNOT_FOUND || sel == 0)
        return;
    wxString item = list->GetString(sel);
    bool checked = list->IsChecked(sel);
    int idx = indices[sel];
    list->Delete(sel);
    list->Insert(item, sel - 1);
    list->Check(sel - 1, checked);
    list->SetSelection(sel - 1);
    indices.erase(indices.begin() + sel);
    indices.insert(indices.begin() + sel - 1, idx);
}

void ColumnSelectionDialog::OnDown(wxCommandEvent&)
{
    int sel = list->GetSelection();
    if (sel == wxNOT_FOUND || sel == static_cast<int>(list->GetCount()) - 1)
        return;
    wxString item = list->GetString(sel);
    bool checked = list->IsChecked(sel);
    int idx = indices[sel];
    list->Delete(sel);
    list->Insert(item, sel + 1);
    list->Check(sel + 1, checked);
    list->SetSelection(sel + 1);
    indices.erase(indices.begin() + sel);
    indices.insert(indices.begin() + sel + 1, idx);
}

std::vector<int> ColumnSelectionDialog::GetSelectedColumns() const
{
    std::vector<int> res;
    for (unsigned int i = 0; i < list->GetCount(); ++i) {
        if (list->IsChecked(i))
            res.push_back(indices[i]);
    }
    return res;
}

void ColumnSelectionDialog::OnSelectAll(wxCommandEvent&)
{
    for (unsigned int i = 0; i < list->GetCount(); ++i)
        list->Check(i, true);
}

void ColumnSelectionDialog::OnDeselectAll(wxCommandEvent&)
{
    for (unsigned int i = 0; i < list->GetCount(); ++i)
        list->Check(i, false);
}
