#pragma once
#include <wx/wx.h>
#include <string>

class GdtfLoginDialog : public wxDialog {
public:
    GdtfLoginDialog(wxWindow* parent, const std::string& user, const std::string& pass);
    std::string GetUsername() const;
    std::string GetPassword() const;
private:
    wxTextCtrl* userCtrl;
    wxTextCtrl* passCtrl;
};
