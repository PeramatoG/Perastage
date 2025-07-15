#include <wx/wx.h>
#include <wx/sysopt.h>
#include "mainwindow.h"

class MyApp : public wxApp
{
public:
    virtual bool OnInit() override;
};

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
    // Enable dark mode for Windows (if supported by wxWidgets)
    wxSystemOptions::SetOption("msw.useDarkMode", 1);

    MainWindow* mainWindow = new MainWindow("Perastage");
    mainWindow->Show(true);
    return true;
}
