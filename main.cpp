/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "mainwindow.h"
#include "projectutils.h"
#include "logger.h"
#include "configmanager.h"
#include <thread>
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

  // Initialize logging system (overwrites log file each launch)
  Logger::Instance();

  // Force dark mode when supported by the wxWidgets version in use
#if wxCHECK_VERSION(3, 3, 0)
  SetAppearance(wxApp::Appearance::Dark);
#endif

  // Enable dark mode for Windows (if supported by wxWidgets)
  wxSystemOptions::SetOption("msw.useDarkMode", 1);

  MainWindow *mainWindow = new MainWindow("Perastage");
  mainWindow->Show(true);
  // Start maximized so minimize and restore buttons remain available
  mainWindow->Maximize(true);

  std::thread([mainWindow]() {
    auto last = ProjectUtils::LoadLastProjectPath();
    bool loaded = false;
    std::string path;
    if (last) {
      path = *last;
      loaded = ConfigManager::Get().LoadProject(path);
    }
    wxCommandEvent evt(EVT_PROJECT_LOADED);
    evt.SetInt(loaded ? 1 : 0);
    evt.SetString(path);
    wxQueueEvent(mainWindow, evt.Clone());
  }).detach();

  return true;
}
