#pragma once
#include <wx/wx.h>
#include <vector>
#include <string>

class SelectNameDialog : public wxDialog {
public:
    SelectNameDialog(wxWindow* parent,
                     const std::vector<std::string>& names,
                     const wxString& title,
                     const wxString& message);
    int GetSelection() const;
private:
    void OnOpen(wxCommandEvent& evt);
    wxListBox* listCtrl = nullptr;
};
