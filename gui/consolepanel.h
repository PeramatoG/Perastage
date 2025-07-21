#pragma once

#include <wx/wx.h>

// Simple panel to display log messages in a console-like view
class ConsolePanel : public wxPanel
{
public:
    explicit ConsolePanel(wxWindow* parent);

    // Append a message to the console
    void AppendMessage(const wxString& msg);

    // Access singleton instance
    static ConsolePanel* Instance();
    static void SetInstance(ConsolePanel* panel);

private:
    wxTextCtrl* m_textCtrl = nullptr;
};
