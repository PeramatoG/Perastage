#pragma once
#include <wx/wx.h>
#include <vector>

class ExportFixtureDialog : public wxDialog {
public:
    ExportFixtureDialog(wxWindow* parent, const std::vector<std::string>& types);
    std::string GetSelectedType() const;
private:
    wxListBox* listBox = nullptr;
};
