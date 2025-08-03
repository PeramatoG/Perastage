#pragma once
#include <wx/wx.h>
#include <vector>
#include <string>

class SelectFixtureTypeDialog : public wxDialog {
public:
    SelectFixtureTypeDialog(wxWindow* parent, const std::vector<std::string>& types);
    int GetSelection() const;
private:
    wxListBox* listCtrl = nullptr;
};
