#pragma once

#include <wx/wx.h>
#include <wx/checklst.h>
#include <vector>
#include <string>

class ColumnSelectionDialog : public wxDialog {
public:
    ColumnSelectionDialog(wxWindow* parent,
                          const std::vector<std::string>& columns,
                          const std::vector<int>& selected = {});
    std::vector<int> GetSelectedColumns() const;

private:
    void OnUp(wxCommandEvent& evt);
    void OnDown(wxCommandEvent& evt);

    wxCheckListBox* list = nullptr;
    std::vector<int> indices; // maps list items to original column indices
};
