#pragma once
#include <wx/wx.h>
#include <vector>

class ExportTrussDialog : public wxDialog {
public:
    ExportTrussDialog(wxWindow* parent, const std::vector<std::string>& names);
    std::string GetSelectedName() const;
private:
    wxListBox* listBox = nullptr;
};
