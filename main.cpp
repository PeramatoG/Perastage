#include "mainwindow.h"
#include "projectutils.h"
#include <wx/sysopt.h>
#include <wx/wx.h>

class MyApp : public wxApp {
public:
  virtual bool OnInit() override;
};

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit() {
  // Enable support for common image formats used by the app
  wxInitAllImageHandlers();

  // Enable dark mode for Windows (if supported by wxWidgets)
  wxSystemOptions::SetOption("msw.useDarkMode", 1);

  MainWindow *mainWindow = new MainWindow("Perastage");
  bool loaded = false;
  if (auto last = ProjectUtils::LoadLastProjectPath())
    loaded = mainWindow->LoadProjectFromPath(*last);
  if (!loaded)
    mainWindow->ResetProject();

  mainWindow->Show(true);
  return true;
}
