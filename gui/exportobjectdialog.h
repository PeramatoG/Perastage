#pragma once
#include <wx/wx.h>
#include <vector>

class ExportObjectDialog : public wxDialog {
public:
    ExportObjectDialog(wxWindow* parent, const std::vector<std::string>& names);
    std::string GetSelectedName() const;
private:
    wxListBox* listBox = nullptr;
};
