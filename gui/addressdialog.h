#pragma once
#include <wx/wx.h>

class AddressDialog : public wxDialog {
public:
    AddressDialog(wxWindow* parent, int universe, int channel);
    int GetUniverse() const;
    int GetChannel() const;
private:
    wxTextCtrl* uniCtrl;
    wxTextCtrl* chCtrl;
};
