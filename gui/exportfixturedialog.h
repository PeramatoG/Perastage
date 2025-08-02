#pragma once
#include <wx/wx.h>
#include <vector>

class ExportFixtureDialog : public wxDialog {
public:
    ExportFixtureDialog(wxWindow* parent, const std::vector<std::string>& names);
    std::string GetSelectedName() const;
private:
    wxListBox* listBox = nullptr;
};
