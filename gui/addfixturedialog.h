#pragma once
#include <wx/wx.h>
#include <wx/spinctrl.h>
#include <vector>

class AddFixtureDialog : public wxDialog {
public:
    AddFixtureDialog(wxWindow* parent,
                     const wxString& defaultName,
                     const std::vector<std::string>& modes);
    int GetUnitCount() const;
    std::string GetFixtureName() const;
    std::string GetMode() const;
private:
    wxSpinCtrl* unitsCtrl = nullptr;
    wxTextCtrl* nameCtrl = nullptr;
    wxChoice* modeCtrl = nullptr;
};
